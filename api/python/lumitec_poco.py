#!/usr/bin/env python3
"""
Lumitec Poco CAN Protocol API - Python Implementation

This module provides a Python implementation of the Lumitec Poco lighting
system CAN protocol that can be used with any CAN stack or raw CAN interface.

The library is designed to be independent of any specific NMEA2000 library
and works directly with raw CAN frame data.

Version: 1.0.0
Author: Lumitec
"""

import struct
from enum import IntEnum
from typing import Optional, Tuple, NamedTuple
from dataclasses import dataclass

# Library version
__version__ = "1.0.0"

# Lumitec Constants
LUMITEC_MANUFACTURER_CODE = 1512
LUMITEC_PGN_61184 = 61184
MARINE_INDUSTRY_CODE = 4  # Standard marine industry code

# Maximum CAN frame data length
LUMITEC_POCO_MAX_DATA_LEN = 8


class PocoProprietaryID(IntEnum):
    """Proprietary IDs for PGN 61184"""
    EXTSW_SIMPLE_ACTIONS = 1        # External Switch: Simple Actions
    EXTSW_STATE_INFO = 2           # External Switch: Switch State Information
    EXTSW_CUSTOM_HSB = 3           # External Switch: Custom Hue, Saturation, Brightness
    EXTSW_START_PATTERN = 4        # External Switch: Start Pattern ID
    OUTPUT_CHANNEL_STATUS = 5      # Poco Output Channel: Status
    OUTPUT_CHANNEL_BIN = 6         # Poco Output Channel: BIN On/Off
    OUTPUT_CHANNEL_PWM = 7         # Poco Output Channel: PWM Dimming
    OUTPUT_CHANNEL_PLI = 8         # Poco Output Channel: PLI Message
    OUTPUT_CHANNEL_PLI_T2HSB = 16  # Poco Output Channel: PLI T2HSB


class PocoActionID(IntEnum):
    """External Switch Action IDs"""
    NO_ACTION = 0
    OFF = 1
    ON = 2
    DIM_DOWN = 3
    DIM_UP = 4
    PATTERN_START = 6
    PATTERN_PAUSE = 7
    T2HSB = 8          # Default Hue, Sat, and Bright
    T2HS = 9           # Default Hue and Sat
    T2B = 10           # Default Bright
    WHITE = 20
    RED = 21
    GREEN = 22
    BLUE = 23
    PLAY_PAUSE = 31
    PATTERN_NEXT = 32
    PATTERN_PREV = 33


class PocoSwitchState(IntEnum):
    """External Switch State values"""
    RELEASED = 0
    PRESSED = 1
    HELD = 2


class PocoSwitchType(IntEnum):
    """External Switch Type values"""
    MOMENTARY = 0
    LATCHING = 1


@dataclass
class PocoCANFrame:
    """CAN Frame structure for Poco messages"""
    can_id: int = 0
    data: bytes = b''
    priority: int = 6
    source_address: int = 0
    destination_address: int = 255

    @property
    def data_length(self) -> int:
        """Get the length of the data payload"""
        return len(self.data)

    def __post_init__(self):
        """Validate data length"""
        if len(self.data) > LUMITEC_POCO_MAX_DATA_LEN:
            raise ValueError(f"Data length {len(self.data)} exceeds maximum {LUMITEC_POCO_MAX_DATA_LEN}")


class PocoSimpleAction(NamedTuple):
    """External Switch Simple Action message data"""
    manufacturer_code: int
    industry_code: int
    proprietary_id: int
    action_id: int
    switch_id: int


class PocoStateInfo(NamedTuple):
    """External Switch State Information message data"""
    manufacturer_code: int
    industry_code: int
    proprietary_id: int
    switch_id: int
    switch_state: int
    switch_type: int


class PocoCustomHSB(NamedTuple):
    """External Switch Custom HSB message data"""
    manufacturer_code: int
    industry_code: int
    proprietary_id: int
    action_id: int
    switch_id: int
    hue: int
    saturation: int
    brightness: int


class PocoStartPattern(NamedTuple):
    """External Switch Start Pattern message data"""
    manufacturer_code: int
    industry_code: int
    proprietary_id: int
    switch_id: int
    pattern_id: int


class LumitecPocoAPI:
    """Main API class for Lumitec Poco CAN Protocol"""

    @staticmethod
    def get_version() -> str:
        """Get library version string"""
        return __version__

    @staticmethod
    def _calculate_nmea2k_can_id(pgn: int, source: int, destination: int, priority: int) -> int:
        """Calculate NMEA2000 CAN ID"""
        can_id = 0
        
        # NMEA2000 29-bit CAN ID format:
        # Bits 28-26: Priority (3 bits)
        # Bit 25: Reserved (1 bit) = 0
        # Bit 24: Data Page (1 bit) = 0 for standard PGNs
        # Bit 23: PDU Format (1 bit) = 1 for PGN >= 240
        # Bits 23-16: PGN high byte
        # Bits 15-8: PGN low byte or destination
        # Bits 7-0: Source address
        
        can_id |= (priority & 0x07) << 26  # Priority
        can_id |= (pgn & 0x3FFFF) << 8     # PGN (18 bits)
        can_id |= source & 0xFF            # Source address
        
        # For PGN 61184, set destination in appropriate bits
        if destination != 255:  # Not broadcast
            can_id |= (destination & 0xFF) << 8
        
        return can_id

    @staticmethod
    def _extract_manufacturer_info(data: bytes) -> Tuple[int, int]:
        """Extract manufacturer and industry codes from data"""
        if len(data) < 2:
            raise ValueError("Data too short for manufacturer info")
        
        # First approach: manufacturer and industry codes in first 2 bytes
        combined = struct.unpack('<H', data[:2])[0]
        manufacturer_code = combined & 0x7FF  # 11 bits
        industry_code = (combined >> 13) & 0x07  # 3 bits
        
        return manufacturer_code, industry_code

    @staticmethod
    def _pack_manufacturer_info(manufacturer_code: int, industry_code: int) -> bytes:
        """Pack manufacturer and industry codes"""
        combined = (manufacturer_code & 0x7FF) | ((industry_code & 0x07) << 13)
        return struct.pack('<H', combined)

    @classmethod
    def is_valid_frame(cls, frame: PocoCANFrame) -> bool:
        """Validate if a CAN frame is a Lumitec Poco message"""
        try:
            # Check minimum data length
            if frame.data_length < 3:
                return False
            
            # Extract and validate manufacturer and industry codes
            manufacturer_code, industry_code = cls._extract_manufacturer_info(frame.data)
            
            return (manufacturer_code == LUMITEC_MANUFACTURER_CODE and 
                   industry_code == MARINE_INDUSTRY_CODE)
        except (ValueError, struct.error):
            return False

    @classmethod
    def get_proprietary_id(cls, frame: PocoCANFrame) -> Optional[int]:
        """Get the proprietary ID from a Lumitec Poco CAN frame"""
        if not cls.is_valid_frame(frame):
            return None
        
        return frame.data[2]

    @classmethod
    def create_simple_action(cls, destination: int, source: int, 
                           action_id: PocoActionID, switch_id: int) -> PocoCANFrame:
        """Create an External Switch Simple Action message"""
        frame = PocoCANFrame()
        frame.can_id = cls._calculate_nmea2k_can_id(LUMITEC_PGN_61184, source, destination, 6)
        frame.priority = 6
        frame.source_address = source
        frame.destination_address = destination
        
        # Pack manufacturer and industry codes
        data = bytearray(cls._pack_manufacturer_info(LUMITEC_MANUFACTURER_CODE, MARINE_INDUSTRY_CODE))
        data.append(PocoProprietaryID.EXTSW_SIMPLE_ACTIONS)
        data.append(action_id)
        data.append(switch_id)
        data.append(0)  # Reserved
        
        frame.data = bytes(data)
        return frame

    @classmethod
    def create_state_info(cls, source: int, switch_id: int, 
                         switch_state: PocoSwitchState, 
                         switch_type: PocoSwitchType) -> PocoCANFrame:
        """Create an External Switch State Information message"""
        frame = PocoCANFrame()
        frame.can_id = cls._calculate_nmea2k_can_id(LUMITEC_PGN_61184, source, 255, 6)  # Broadcast
        frame.priority = 6
        frame.source_address = source
        frame.destination_address = 255  # Broadcast
        
        # Pack manufacturer and industry codes
        data = bytearray(cls._pack_manufacturer_info(LUMITEC_MANUFACTURER_CODE, MARINE_INDUSTRY_CODE))
        data.append(PocoProprietaryID.EXTSW_STATE_INFO)
        data.append(switch_id)
        data.append(switch_state)
        data.append(switch_type)
        data.append(0)  # Reserved
        
        frame.data = bytes(data)
        return frame

    @classmethod
    def create_custom_hsb(cls, destination: int, source: int, 
                         action_id: PocoActionID, switch_id: int,
                         hue: int, saturation: int, brightness: int) -> PocoCANFrame:
        """Create an External Switch Custom HSB message"""
        frame = PocoCANFrame()
        frame.can_id = cls._calculate_nmea2k_can_id(LUMITEC_PGN_61184, source, destination, 6)
        frame.priority = 6
        frame.source_address = source
        frame.destination_address = destination
        
        # Pack manufacturer and industry codes
        data = bytearray(cls._pack_manufacturer_info(LUMITEC_MANUFACTURER_CODE, MARINE_INDUSTRY_CODE))
        data.append(PocoProprietaryID.EXTSW_CUSTOM_HSB)
        data.append(action_id)
        data.append(switch_id)
        data.append(hue)
        data.append(saturation)
        data.append(brightness)
        
        frame.data = bytes(data)
        return frame

    @classmethod
    def create_start_pattern(cls, destination: int, source: int, 
                           switch_id: int, pattern_id: int) -> PocoCANFrame:
        """Create an External Switch Start Pattern message"""
        frame = PocoCANFrame()
        frame.can_id = cls._calculate_nmea2k_can_id(LUMITEC_PGN_61184, source, destination, 6)
        frame.priority = 6
        frame.source_address = source
        frame.destination_address = destination
        
        # Pack manufacturer and industry codes
        data = bytearray(cls._pack_manufacturer_info(LUMITEC_MANUFACTURER_CODE, MARINE_INDUSTRY_CODE))
        data.append(PocoProprietaryID.EXTSW_START_PATTERN)
        data.append(switch_id)
        data.append(pattern_id)
        data.append(0)  # Reserved
        
        frame.data = bytes(data)
        return frame

    @classmethod
    def parse_simple_action(cls, frame: PocoCANFrame) -> Optional[PocoSimpleAction]:
        """Parse an External Switch Simple Action message"""
        proprietary_id = cls.get_proprietary_id(frame)
        if (proprietary_id != PocoProprietaryID.EXTSW_SIMPLE_ACTIONS or
            frame.data_length < 6):
            return None
        
        manufacturer_code, industry_code = cls._extract_manufacturer_info(frame.data)
        
        return PocoSimpleAction(
            manufacturer_code=manufacturer_code,
            industry_code=industry_code,
            proprietary_id=frame.data[2],
            action_id=frame.data[3],
            switch_id=frame.data[4]
        )

    @classmethod
    def parse_state_info(cls, frame: PocoCANFrame) -> Optional[PocoStateInfo]:
        """Parse an External Switch State Information message"""
        proprietary_id = cls.get_proprietary_id(frame)
        if (proprietary_id != PocoProprietaryID.EXTSW_STATE_INFO or
            frame.data_length < 7):
            return None
        
        manufacturer_code, industry_code = cls._extract_manufacturer_info(frame.data)
        
        return PocoStateInfo(
            manufacturer_code=manufacturer_code,
            industry_code=industry_code,
            proprietary_id=frame.data[2],
            switch_id=frame.data[3],
            switch_state=frame.data[4],
            switch_type=frame.data[5]
        )

    @classmethod
    def parse_custom_hsb(cls, frame: PocoCANFrame) -> Optional[PocoCustomHSB]:
        """Parse an External Switch Custom HSB message"""
        proprietary_id = cls.get_proprietary_id(frame)
        if (proprietary_id != PocoProprietaryID.EXTSW_CUSTOM_HSB or
            frame.data_length < 8):
            return None
        
        manufacturer_code, industry_code = cls._extract_manufacturer_info(frame.data)
        
        return PocoCustomHSB(
            manufacturer_code=manufacturer_code,
            industry_code=industry_code,
            proprietary_id=frame.data[2],
            action_id=frame.data[3],
            switch_id=frame.data[4],
            hue=frame.data[5],
            saturation=frame.data[6],
            brightness=frame.data[7]
        )

    @classmethod
    def parse_start_pattern(cls, frame: PocoCANFrame) -> Optional[PocoStartPattern]:
        """Parse an External Switch Start Pattern message"""
        proprietary_id = cls.get_proprietary_id(frame)
        if (proprietary_id != PocoProprietaryID.EXTSW_START_PATTERN or
            frame.data_length < 6):
            return None
        
        manufacturer_code, industry_code = cls._extract_manufacturer_info(frame.data)
        
        return PocoStartPattern(
            manufacturer_code=manufacturer_code,
            industry_code=industry_code,
            proprietary_id=frame.data[2],
            switch_id=frame.data[3],
            pattern_id=frame.data[4]
        )

    @staticmethod
    def action_to_string(action_id: PocoActionID) -> str:
        """Convert action ID to human-readable string"""
        action_names = {
            PocoActionID.NO_ACTION: "No Action",
            PocoActionID.OFF: "Off",
            PocoActionID.ON: "On",
            PocoActionID.DIM_DOWN: "Dim Down",
            PocoActionID.DIM_UP: "Dim Up",
            PocoActionID.PATTERN_START: "Pattern Start",
            PocoActionID.PATTERN_PAUSE: "Pattern Pause",
            PocoActionID.T2HSB: "T2HSB",
            PocoActionID.T2HS: "T2HS",
            PocoActionID.T2B: "T2B",
            PocoActionID.WHITE: "White",
            PocoActionID.RED: "Red",
            PocoActionID.GREEN: "Green",
            PocoActionID.BLUE: "Blue",
            PocoActionID.PLAY_PAUSE: "Play/Pause",
            PocoActionID.PATTERN_NEXT: "Pattern Next",
            PocoActionID.PATTERN_PREV: "Pattern Previous"
        }
        return action_names.get(action_id, "Unknown")

    @staticmethod
    def state_to_string(state: PocoSwitchState) -> str:
        """Convert switch state to human-readable string"""
        state_names = {
            PocoSwitchState.RELEASED: "Released",
            PocoSwitchState.PRESSED: "Pressed",
            PocoSwitchState.HELD: "Held"
        }
        return state_names.get(state, "Unknown")

    @staticmethod
    def type_to_string(switch_type: PocoSwitchType) -> str:
        """Convert switch type to human-readable string"""
        type_names = {
            PocoSwitchType.MOMENTARY: "Momentary",
            PocoSwitchType.LATCHING: "Latching"
        }
        return type_names.get(switch_type, "Unknown")


# Convenience functions for easier usage
def create_simple_action(destination: int, source: int, 
                        action_id: PocoActionID, switch_id: int) -> PocoCANFrame:
    """Convenience function to create a simple action message"""
    return LumitecPocoAPI.create_simple_action(destination, source, action_id, switch_id)


def create_custom_hsb(destination: int, source: int, action_id: PocoActionID, 
                     switch_id: int, hue: int, saturation: int, brightness: int) -> PocoCANFrame:
    """Convenience function to create a custom HSB message"""
    return LumitecPocoAPI.create_custom_hsb(destination, source, action_id, switch_id, hue, saturation, brightness)


def create_state_info(source: int, switch_id: int, 
                     switch_state: PocoSwitchState, switch_type: PocoSwitchType) -> PocoCANFrame:
    """Convenience function to create a state info message"""
    return LumitecPocoAPI.create_state_info(source, switch_id, switch_state, switch_type)


def create_start_pattern(destination: int, source: int, 
                        switch_id: int, pattern_id: int) -> PocoCANFrame:
    """Convenience function to create a start pattern message"""
    return LumitecPocoAPI.create_start_pattern(destination, source, switch_id, pattern_id)


def is_valid_poco_frame(frame: PocoCANFrame) -> bool:
    """Convenience function to validate a Poco frame"""
    return LumitecPocoAPI.is_valid_frame(frame)


def parse_poco_frame(frame: PocoCANFrame):
    """Parse a Poco frame and return the appropriate message type"""
    proprietary_id = LumitecPocoAPI.get_proprietary_id(frame)
    
    if proprietary_id == PocoProprietaryID.EXTSW_SIMPLE_ACTIONS:
        return LumitecPocoAPI.parse_simple_action(frame)
    elif proprietary_id == PocoProprietaryID.EXTSW_STATE_INFO:
        return LumitecPocoAPI.parse_state_info(frame)
    elif proprietary_id == PocoProprietaryID.EXTSW_CUSTOM_HSB:
        return LumitecPocoAPI.parse_custom_hsb(frame)
    elif proprietary_id == PocoProprietaryID.EXTSW_START_PATTERN:
        return LumitecPocoAPI.parse_start_pattern(frame)
    else:
        return None


if __name__ == "__main__":
    # Simple test when run directly
    print(f"Lumitec Poco CAN API Python Version {LumitecPocoAPI.get_version()}")
    
    # Create a test frame
    frame = create_simple_action(0x0E, 0x10, PocoActionID.ON, 1)
    print(f"Created frame: CAN ID=0x{frame.can_id:08X}, Data={frame.data.hex().upper()}")
    
    # Parse it back
    action = LumitecPocoAPI.parse_simple_action(frame)
    if action:
        print(f"Parsed: {LumitecPocoAPI.action_to_string(PocoActionID(action.action_id))}, Switch {action.switch_id}")

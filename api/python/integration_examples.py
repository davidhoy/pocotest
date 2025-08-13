#!/usr/bin/env python3
"""
Integration examples for using Lumitec Poco API with popular Python CAN libraries

This file shows how to integrate the Lumitec Poco API with different
Python CAN interfaces and libraries.
"""

from lumitec_poco import (
    PocoCANFrame, PocoActionID, create_simple_action, create_custom_hsb,
    is_valid_poco_frame, parse_poco_frame
)


# Example 1: Integration with python-can library
def example_python_can():
    """Example using python-can library (pip install python-can)"""
    try:
        import can
        
        print("=== Python-CAN Integration Example ===")
        
        # Create a Poco message
        poco_frame = create_simple_action(0x0E, 0x10, PocoActionID.ON, 1)
        
        # Convert to python-can Message
        can_msg = can.Message(
            arbitration_id=poco_frame.can_id,
            data=poco_frame.data,
            is_extended_id=True  # NMEA2000 uses 29-bit extended IDs
        )
        
        print(f"Created CAN message: ID=0x{can_msg.arbitration_id:08X}, Data={can_msg.data.hex().upper()}")
        
        # Simulate sending (replace with your actual CAN bus)
        # bus = can.interface.Bus(channel='vcan0', bustype='socketcan')
        # bus.send(can_msg)
        
        # Convert received message back to Poco frame
        received_poco = PocoCANFrame(
            can_id=can_msg.arbitration_id,
            data=can_msg.data,
            source_address=can_msg.arbitration_id & 0xFF,
            destination_address=(can_msg.arbitration_id >> 8) & 0xFF if (can_msg.arbitration_id >> 8) & 0xFF != 0xFF else 255
        )
        
        if is_valid_poco_frame(received_poco):
            parsed = parse_poco_frame(received_poco)
            print(f"Parsed Poco message: {parsed}")
        
    except ImportError:
        print("python-can library not installed. Run: pip install python-can")


# Example 2: Integration with SocketCAN directly
def example_socketcan():
    """Example using raw SocketCAN (Linux only)"""
    try:
        import socket
        import struct
        
        print("\n=== SocketCAN Integration Example ===")
        
        # Create a Poco message
        poco_frame = create_custom_hsb(0x0E, 0x10, PocoActionID.T2HSB, 1, 128, 255, 200)
        
        # Convert to SocketCAN frame format
        can_id = poco_frame.can_id | 0x80000000  # Set extended frame flag
        can_dlc = poco_frame.data_length
        can_data = poco_frame.data.ljust(8, b'\x00')  # Pad to 8 bytes
        
        # SocketCAN frame format: ID(4) + DLC(1) + PAD(3) + DATA(8)
        can_frame = struct.pack("=IB3x8s", can_id, can_dlc, can_data)
        
        print(f"SocketCAN frame: {can_frame.hex().upper()}")
        
        # Simulate sending (uncomment to actually send)
        # sock = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
        # sock.bind(('vcan0',))
        # sock.send(can_frame)
        # sock.close()
        
        # Parse received frame
        received_id, received_dlc = struct.unpack("=IB3x", can_frame[:8])
        received_data = can_frame[8:8+received_dlc]
        
        received_poco = PocoCANFrame(
            can_id=received_id & 0x1FFFFFFF,  # Remove extended flag
            data=received_data,
            source_address=received_id & 0xFF
        )
        
        if is_valid_poco_frame(received_poco):
            parsed = parse_poco_frame(received_poco)
            print(f"Parsed Poco message: {parsed}")
            
    except ImportError:
        print("SocketCAN not available (Linux only)")


# Example 3: Integration with PCAN-Basic API
def example_pcan():
    """Example using PEAK PCAN-Basic API"""
    try:
        # Note: This requires PCAN-Basic library installation
        # from pypcan import PCANBasic, PCAN_USBBUS1, PCAN_BAUD_250K
        
        print("\n=== PCAN Integration Example ===")
        
        # Create a Poco message
        poco_frame = create_simple_action(0x0E, 0x10, PocoActionID.DIM_UP, 2)
        
        # Convert to PCAN message format
        pcan_msg = {
            'ID': poco_frame.can_id,
            'MSGTYPE': 0x02,  # PCAN_MESSAGE_EXTENDED
            'LEN': poco_frame.data_length,
            'DATA': list(poco_frame.data) + [0] * (8 - poco_frame.data_length)
        }
        
        print(f"PCAN message: ID=0x{pcan_msg['ID']:08X}, Data={pcan_msg['DATA'][:pcan_msg['LEN']]}")
        
        # Simulate sending (uncomment and install pypcan)
        # pcan = PCANBasic()
        # result = pcan.Write(PCAN_USBBUS1, pcan_msg)
        
        # Convert back to Poco frame
        received_poco = PocoCANFrame(
            can_id=pcan_msg['ID'],
            data=bytes(pcan_msg['DATA'][:pcan_msg['LEN']]),
            source_address=pcan_msg['ID'] & 0xFF
        )
        
        if is_valid_poco_frame(received_poco):
            parsed = parse_poco_frame(received_poco)
            print(f"Parsed Poco message: {parsed}")
            
    except ImportError:
        print("PCAN library not available. Install pypcan or pcan-basic")


# Example 4: Integration with CANtact/gs_usb devices
def example_cantact():
    """Example using CANtact/gs_usb devices via python-can"""
    try:
        import can
        
        print("\n=== CANtact/gs_usb Integration Example ===")
        
        # Create multiple Poco messages
        messages = [
            create_simple_action(0x0E, 0x10, PocoActionID.ON, 1),
            create_simple_action(0x0E, 0x10, PocoActionID.OFF, 1),
            create_custom_hsb(0x0E, 0x10, PocoActionID.T2HSB, 1, 0, 255, 255),  # Red
            create_custom_hsb(0x0E, 0x10, PocoActionID.T2HSB, 1, 85, 255, 255), # Green
            create_custom_hsb(0x0E, 0x10, PocoActionID.T2HSB, 1, 170, 255, 255) # Blue
        ]
        
        for i, poco_frame in enumerate(messages):
            can_msg = can.Message(
                arbitration_id=poco_frame.can_id,
                data=poco_frame.data,
                is_extended_id=True
            )
            
            print(f"Message {i+1}: ID=0x{can_msg.arbitration_id:08X}, Data={can_msg.data.hex().upper()}")
            
            # Simulate sending via CANtact (uncomment to actually send)
            # bus = can.interface.Bus(channel='COM3', bustype='cantact')  # Windows
            # bus = can.interface.Bus(channel='/dev/ttyACM0', bustype='cantact')  # Linux
            # bus.send(can_msg)
        
    except ImportError:
        print("python-can library not installed")


# Example 5: NMEA2000 message filtering
def example_message_filtering():
    """Example of filtering NMEA2000 messages for Poco messages"""
    print("\n=== NMEA2000 Message Filtering Example ===")
    
    # Simulate received NMEA2000 messages
    simulated_messages = [
        # Poco message
        PocoCANFrame(can_id=0x18EF0E10, data=b'\xE8\x85\x01\x02\x01\x00'),
        # Non-Poco message (different PGN)
        PocoCANFrame(can_id=0x18FF0010, data=b'\x01\x02\x03\x04\x05\x06'),
        # Another Poco message
        PocoCANFrame(can_id=0x18EF0E10, data=b'\xE8\x85\x03\x08\x01\x80\xFF\xC8'),
        # Invalid Poco message (wrong manufacturer)
        PocoCANFrame(can_id=0x18EF0E10, data=b'\x00\x00\x01\x02\x01\x00')
    ]
    
    poco_count = 0
    for i, frame in enumerate(simulated_messages):
        if is_valid_poco_frame(frame):
            parsed = parse_poco_frame(frame)
            print(f"Message {i+1}: Valid Poco message - {type(parsed).__name__}")
            print(f"  Content: {parsed}")
            poco_count += 1
        else:
            print(f"Message {i+1}: Not a Poco message")
    
    print(f"\nFound {poco_count} valid Poco messages out of {len(simulated_messages)} total")


# Example 6: Batch message processing
def example_batch_processing():
    """Example of processing multiple Poco messages efficiently"""
    print("\n=== Batch Message Processing Example ===")
    
    # Create a batch of different Poco messages
    batch = []
    
    # Simple actions for different switches
    for switch_id in range(1, 5):
        batch.append(create_simple_action(0x0E, 0x10, PocoActionID.ON, switch_id))
    
    # HSB colors - rainbow effect
    colors = [(0, 255, 255), (42, 255, 255), (85, 255, 255), (127, 255, 255), (170, 255, 255)]
    for i, (h, s, b) in enumerate(colors):
        batch.append(create_custom_hsb(0x0E, 0x10, PocoActionID.T2HSB, 1, h, s, b))
    
    print(f"Created batch of {len(batch)} Poco messages")
    
    # Process batch
    action_count = 0
    hsb_count = 0
    
    for frame in batch:
        parsed = parse_poco_frame(frame)
        if hasattr(parsed, 'action_id'):
            if hasattr(parsed, 'hue'):  # HSB message
                hsb_count += 1
            else:  # Simple action
                action_count += 1
    
    print(f"Processed: {action_count} simple actions, {hsb_count} HSB commands")


def main():
    """Run all integration examples"""
    print("Lumitec Poco CAN API - Integration Examples")
    print("=" * 50)
    
    # Run all examples
    example_python_can()
    example_socketcan()
    example_pcan()
    example_cantact()
    example_message_filtering()
    example_batch_processing()
    
    print("\n" + "=" * 50)
    print("Integration examples completed!")
    print("\nTo use with real CAN hardware:")
    print("1. Install appropriate CAN library (python-can, pypcan, etc.)")
    print("2. Configure your CAN interface")
    print("3. Uncomment the actual send/receive code")
    print("4. Replace channel/device names with your hardware")


if __name__ == "__main__":
    main()

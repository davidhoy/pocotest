#!/usr/bin/env python3
"""
Example usage of the Lumitec Poco CAN API Python library

This example demonstrates how to use the Python Lumitec Poco API to create
and parse CAN messages for controlling Lumitec lighting systems.
"""

from lumitec_poco import (
    LumitecPocoAPI, PocoCANFrame, PocoActionID, PocoSwitchState, PocoSwitchType,
    create_simple_action, create_custom_hsb, create_state_info, create_start_pattern,
    is_valid_poco_frame, parse_poco_frame
)


def print_can_frame(frame: PocoCANFrame) -> None:
    """Print a CAN frame in hex format"""
    data_hex = frame.data.hex().upper()
    data_hex_spaced = ' '.join(data_hex[i:i+2] for i in range(0, len(data_hex), 2))
    print(f"CAN ID: 0x{frame.can_id:08X}, DLC: {frame.data_length}, Data: {data_hex_spaced}")


def main():
    print("Lumitec Poco CAN API Python Example")
    print(f"API Version: {LumitecPocoAPI.get_version()}\n")
    
    # Example 1: Create a simple action message
    print("=== Example 1: Simple Action Message ===")
    frame = create_simple_action(0x0E, 0x10, PocoActionID.ON, 1)
    print("Created simple action message (Turn On, Switch 1):")
    print_can_frame(frame)
    
    # Parse the message back
    action = LumitecPocoAPI.parse_simple_action(frame)
    if action:
        print(f"Parsed: Action={LumitecPocoAPI.action_to_string(PocoActionID(action.action_id))}, Switch={action.switch_id}")
    print()
    
    # Example 2: Create a custom HSB message
    print("=== Example 2: Custom HSB Message ===")
    frame = create_custom_hsb(0x0E, 0x10, PocoActionID.T2HSB, 1, 128, 255, 200)
    print("Created custom HSB message (Hue=128, Sat=255, Bright=200):")
    print_can_frame(frame)
    
    # Parse the message back
    hsb = LumitecPocoAPI.parse_custom_hsb(frame)
    if hsb:
        print(f"Parsed: Action={LumitecPocoAPI.action_to_string(PocoActionID(hsb.action_id))}, "
              f"Switch={hsb.switch_id}, H={hsb.hue}, S={hsb.saturation}, B={hsb.brightness}")
    print()
    
    # Example 3: Create a state information message
    print("=== Example 3: State Information Message ===")
    frame = create_state_info(0x10, 2, PocoSwitchState.PRESSED, PocoSwitchType.MOMENTARY)
    print("Created state info message (Switch 2 pressed, momentary):")
    print_can_frame(frame)
    
    # Parse the message back
    state_info = LumitecPocoAPI.parse_state_info(frame)
    if state_info:
        print(f"Parsed: Switch={state_info.switch_id}, "
              f"State={LumitecPocoAPI.state_to_string(PocoSwitchState(state_info.switch_state))}, "
              f"Type={LumitecPocoAPI.type_to_string(PocoSwitchType(state_info.switch_type))}")
    print()
    
    # Example 4: Create a start pattern message
    print("=== Example 4: Start Pattern Message ===")
    frame = create_start_pattern(0x0E, 0x10, 1, 5)
    print("Created start pattern message (Switch 1, Pattern 5):")
    print_can_frame(frame)
    
    # Parse the message back
    pattern = LumitecPocoAPI.parse_start_pattern(frame)
    if pattern:
        print(f"Parsed: Switch={pattern.switch_id}, Pattern={pattern.pattern_id}")
    print()
    
    # Example 5: Validate frame and auto-parse
    print("=== Example 5: Frame Validation and Auto-parsing ===")
    print(f"Frame is valid Lumitec Poco message: {is_valid_poco_frame(frame)}")
    
    prop_id = LumitecPocoAPI.get_proprietary_id(frame)
    if prop_id:
        print(f"Proprietary ID: {prop_id}")
    
    # Auto-parse any Poco frame
    parsed_msg = parse_poco_frame(frame)
    if parsed_msg:
        print(f"Auto-parsed message type: {type(parsed_msg).__name__}")
        print(f"Message content: {parsed_msg}")
    print()
    
    # Example 6: Working with different action types
    print("=== Example 6: Different Action Types ===")
    actions = [
        (PocoActionID.OFF, "Turn Off"),
        (PocoActionID.DIM_UP, "Dim Up"),
        (PocoActionID.RED, "Red Color"),
        (PocoActionID.PATTERN_START, "Start Pattern")
    ]
    
    for action_id, description in actions:
        frame = create_simple_action(0x0E, 0x10, action_id, 1)
        print(f"{description}: {frame.data.hex().upper()}")
    print()
    
    # Example 7: HSB Color Examples
    print("=== Example 7: HSB Color Examples ===")
    colors = [
        (0, 255, 255, "Pure Red"),
        (85, 255, 255, "Pure Green"),
        (170, 255, 255, "Pure Blue"),
        (0, 0, 255, "White"),
        (42, 255, 200, "Orange")
    ]
    
    for hue, sat, bright, description in colors:
        frame = create_custom_hsb(0x0E, 0x10, PocoActionID.T2HSB, 1, hue, sat, bright)
        print(f"{description} (H={hue}, S={sat}, B={bright}): {frame.data.hex().upper()}")


if __name__ == "__main__":
    main()

# Lumitec Poco CAN API - Python Library

A Python implementation of the Lumitec Poco lighting system CAN protocol. This library provides a clean, Pythonic interface for creating and parsing Poco CAN messages, making it easy to integrate Poco lighting control into Python applications.

## Features

- 🐍 **Pure Python** - No external dependencies required
- 🚀 **Easy to Use** - Intuitive Python API with type hints
- 🔧 **CAN Stack Agnostic** - Works with any Python CAN library
- ✅ **Comprehensive** - Supports all Poco External Switch commands
- 🧪 **Well Tested** - Extensive test suite with >95% coverage
- 📚 **Well Documented** - Complete API documentation and examples

## Installation

### From PyPI (when available)

```bash
pip install lumitec-poco
```

### From Source

```bash
git clone https://github.com/lumitec/poco-api.git
cd poco-api/python
pip install -e .
```

### Development Installation

```bash
pip install -e ".[dev]"
```

## Quick Start

```python
from lumitec_poco import create_simple_action, PocoActionID, parse_poco_frame

# Create a simple "turn on" command
frame = create_simple_action(
    destination=0x0E,    # Target device address
    source=0x10,         # Your device address
    action_id=PocoActionID.ON,
    switch_id=1
)

print(f"CAN ID: 0x{frame.can_id:08X}")
print(f"Data: {frame.data.hex().upper()}")

# Parse received messages
if frame.is_valid_poco_frame():
    parsed = parse_poco_frame(frame)
    print(f"Parsed: {parsed}")
```

## Supported Message Types

### Simple Actions

Control basic lighting functions:

```python
from lumitec_poco import create_simple_action, PocoActionID

# Turn lights on/off
frame = create_simple_action(0x0E, 0x10, PocoActionID.ON, switch_id=1)
frame = create_simple_action(0x0E, 0x10, PocoActionID.OFF, switch_id=1)

# Dimming
frame = create_simple_action(0x0E, 0x10, PocoActionID.DIM_UP, switch_id=1)
frame = create_simple_action(0x0E, 0x10, PocoActionID.DIM_DOWN, switch_id=1)

# Colors
frame = create_simple_action(0x0E, 0x10, PocoActionID.RED, switch_id=1)
frame = create_simple_action(0x0E, 0x10, PocoActionID.WHITE, switch_id=1)
```

### Custom HSB (Hue, Saturation, Brightness)

Precise color control:

```python
from lumitec_poco import create_custom_hsb, PocoActionID

# Pure red (hue=0, full saturation and brightness)
frame = create_custom_hsb(0x0E, 0x10, PocoActionID.T2HSB, 1, 
                         hue=0, saturation=255, brightness=255)

# Pure green
frame = create_custom_hsb(0x0E, 0x10, PocoActionID.T2HSB, 1,
                         hue=85, saturation=255, brightness=255)

# Warm white (low saturation)
frame = create_custom_hsb(0x0E, 0x10, PocoActionID.T2HSB, 1,
                         hue=30, saturation=50, brightness=200)
```

### Switch State Information

Report switch status:

```python
from lumitec_poco import create_state_info, PocoSwitchState, PocoSwitchType

frame = create_state_info(
    source=0x10,
    switch_id=1,
    switch_state=PocoSwitchState.PRESSED,
    switch_type=PocoSwitchType.MOMENTARY
)
```

### Pattern Control

Start lighting patterns:

```python
from lumitec_poco import create_start_pattern

frame = create_start_pattern(
    destination=0x0E,
    source=0x10,
    switch_id=1,
    pattern_id=5  # Pattern number to start
)
```

## CAN Integration Examples

### With python-can library

```python
import can
from lumitec_poco import create_simple_action, PocoActionID

# Create Poco message
poco_frame = create_simple_action(0x0E, 0x10, PocoActionID.ON, 1)

# Convert to python-can Message
can_msg = can.Message(
    arbitration_id=poco_frame.can_id,
    data=poco_frame.data,
    is_extended_id=True
)

# Send via CAN bus
bus = can.interface.Bus(channel='vcan0', bustype='socketcan')
bus.send(can_msg)
```

### With SocketCAN (Linux)

```python
import socket
import struct
from lumitec_poco import create_custom_hsb, PocoActionID

# Create Poco message
poco_frame = create_custom_hsb(0x0E, 0x10, PocoActionID.T2HSB, 1, 128, 255, 200)

# Convert to SocketCAN format
can_id = poco_frame.can_id | 0x80000000  # Extended frame
can_frame = struct.pack("=IB3x8s", can_id, len(poco_frame.data), 
                       poco_frame.data.ljust(8, b'\x00'))

# Send via SocketCAN
sock = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
sock.bind(('can0',))
sock.send(can_frame)
sock.close()
```

## Message Parsing

### Automatic parsing

```python
from lumitec_poco import parse_poco_frame, is_valid_poco_frame

# Check if frame is a valid Poco message
if is_valid_poco_frame(received_frame):
    # Automatically detect and parse message type
    parsed = parse_poco_frame(received_frame)
    
    if hasattr(parsed, 'hue'):  # HSB message
        print(f"HSB: H={parsed.hue}, S={parsed.saturation}, B={parsed.brightness}")
    elif hasattr(parsed, 'action_id'):  # Simple action
        print(f"Action: {parsed.action_id}, Switch: {parsed.switch_id}")
```

### Manual parsing

```python
from lumitec_poco import LumitecPocoAPI, PocoProprietaryID

prop_id = LumitecPocoAPI.get_proprietary_id(frame)

if prop_id == PocoProprietaryID.EXTSW_SIMPLE_ACTIONS:
    action = LumitecPocoAPI.parse_simple_action(frame)
    print(f"Simple action: {action}")
elif prop_id == PocoProprietaryID.EXTSW_CUSTOM_HSB:
    hsb = LumitecPocoAPI.parse_custom_hsb(frame)
    print(f"Custom HSB: {hsb}")
```

## Advanced Usage

### Batch Processing

```python
from lumitec_poco import create_simple_action, PocoActionID

# Create multiple messages
messages = []
for switch_id in range(1, 5):
    frame = create_simple_action(0x0E, 0x10, PocoActionID.ON, switch_id)
    messages.append(frame)

# Send all messages
for frame in messages:
    # Send via your CAN interface
    pass
```

### Message Filtering

```python
from lumitec_poco import is_valid_poco_frame, parse_poco_frame

def process_can_messages(can_messages):
    """Process a list of CAN messages, filtering for Poco messages"""
    poco_messages = []
    
    for can_msg in can_messages:
        # Convert CAN message to Poco frame format
        poco_frame = PocoCANFrame(
            can_id=can_msg.arbitration_id,
            data=can_msg.data
        )
        
        # Filter for valid Poco messages
        if is_valid_poco_frame(poco_frame):
            parsed = parse_poco_frame(poco_frame)
            poco_messages.append(parsed)
    
    return poco_messages
```

## API Reference

### Core Functions

- `create_simple_action()` - Create simple on/off/dim commands
- `create_custom_hsb()` - Create HSB color commands  
- `create_state_info()` - Create switch state messages
- `create_start_pattern()` - Create pattern control messages
- `is_valid_poco_frame()` - Validate Poco message
- `parse_poco_frame()` - Auto-parse any Poco message

### Enums

- `PocoActionID` - Available actions (ON, OFF, DIM_UP, etc.)
- `PocoSwitchState` - Switch states (PRESSED, RELEASED, HELD)
- `PocoSwitchType` - Switch types (MOMENTARY, LATCHING)
- `PocoProprietaryID` - Message types

### Data Classes

- `PocoCANFrame` - CAN frame representation
- `PocoSimpleAction` - Parsed simple action data
- `PocoCustomHSB` - Parsed HSB data
- `PocoStateInfo` - Parsed state info data
- `PocoStartPattern` - Parsed pattern data

## Testing

Run the test suite:

```bash
python -m pytest test_poco_api.py -v
```

Or run the built-in test:

```bash
python test_poco_api.py
```

## Examples

See the `examples/` directory for complete working examples:

- `example_usage.py` - Basic usage examples
- `integration_examples.py` - CAN library integration
- `test_poco_api.py` - Comprehensive test suite

## Requirements

- Python 3.7+
- No required dependencies (uses only Python standard library)

### Optional Dependencies

- `python-can` - For CAN bus communication
- `pypcan` - For PEAK CAN interfaces
- `pytest` - For running tests

## License

This library is provided by Lumitec for use with Lumitec Poco lighting systems.

## Support

For technical support or questions about the Poco protocol:

- Email: <support@lumitec.com>
- Documentation: <https://lumitec.com/poco-api-docs>
- Issues: <https://github.com/lumitec/poco-api/issues>

## Version History

- v1.0.0 - Initial Python implementation
  - Complete Poco protocol support
  - Pure Python implementation
  - Comprehensive test suite
  - Integration examples

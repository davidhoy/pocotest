# Lumitec Poco CAN API Library

A portable C library for implementing the Lumitec Poco lighting system CAN protocol. This library is designed to work with any CAN stack or raw CAN interface, making it easy to integrate Poco lighting control into various systems.

## Features

- **Portable**: Works with any CAN stack or raw CAN interface
- **Lightweight**: No dependencies on specific NMEA2000 libraries
- **Complete**: Supports all Poco External Switch commands
- **Easy to Use**: Simple C API with clear function names
- **Well Documented**: Comprehensive documentation and examples

## Supported Messages

The library supports the following Lumitec Poco protocol messages (PGN 61184):

- **Simple Actions**: Basic on/off/dim commands
- **State Information**: Switch state reporting  
- **Custom HSB**: Hue, Saturation, Brightness control
- **Start Pattern**: Pattern playback control

## Quick Start

### Building the Library

```bash
cd api/
make all
```

This creates:
- `liblumitec_poco.a` - Static library
- `liblumitec_poco.so` - Shared library  
- `example_usage` - Example program

### Running the Example

```bash
make test
```

### Basic Usage

```c
#include "lumitec_poco_api.h"

// Create a CAN frame for turning on a light
lumitec_poco_can_frame_t frame;
lumitec_poco_create_simple_action(&frame, 0x0E, 0x10, POCO_ACTION_ON, 1);

// Send the frame using your CAN interface
// can_send(&frame);

// Parse received frames
if (lumitec_poco_is_valid_frame(&received_frame)) {
    uint8_t prop_id;
    lumitec_poco_get_proprietary_id(&received_frame, &prop_id);
    
    if (prop_id == POCO_PID_EXTSW_SIMPLE_ACTIONS) {
        lumitec_poco_simple_action_t action;
        lumitec_poco_parse_simple_action(&received_frame, &action);
        printf("Action: %s, Switch: %d\\n", 
               lumitec_poco_action_to_string(action.action_id),
               action.switch_id);
    }
}
```

## API Reference

### Core Functions

- `lumitec_poco_get_version()` - Get library version
- `lumitec_poco_is_valid_frame()` - Validate Poco message
- `lumitec_poco_get_proprietary_id()` - Extract message type

### Message Creation

- `lumitec_poco_create_simple_action()` - Create simple on/off/dim commands
- `lumitec_poco_create_custom_hsb()` - Create HSB color commands
- `lumitec_poco_create_state_info()` - Create switch state messages
- `lumitec_poco_create_start_pattern()` - Create pattern control messages

### Message Parsing

- `lumitec_poco_parse_simple_action()` - Parse simple action messages
- `lumitec_poco_parse_custom_hsb()` - Parse HSB color messages
- `lumitec_poco_parse_state_info()` - Parse switch state messages
- `lumitec_poco_parse_start_pattern()` - Parse pattern control messages

### Utility Functions

- `lumitec_poco_action_to_string()` - Convert action ID to string
- `lumitec_poco_state_to_string()` - Convert switch state to string
- `lumitec_poco_type_to_string()` - Convert switch type to string

## Integration with Different CAN Stacks

### Raw SocketCAN (Linux)

```c
#include <sys/socket.h>
#include <linux/can.h>

// Convert Poco frame to SocketCAN frame
struct can_frame can_frame;
can_frame.can_id = poco_frame.can_id | CAN_EFF_FLAG;  // Extended frame
can_frame.can_dlc = poco_frame.data_length;
memcpy(can_frame.data, poco_frame.data, poco_frame.data_length);

// Send via SocketCAN
write(can_socket, &can_frame, sizeof(can_frame));
```

### PEAK CAN API

```c
#include "PCANBasic.h"

// Convert to PEAK format
TPCANMsg msg;
msg.ID = poco_frame.can_id;
msg.MSGTYPE = PCAN_MESSAGE_EXTENDED;
msg.LEN = poco_frame.data_length;
memcpy(msg.DATA, poco_frame.data, poco_frame.data_length);

// Send via PEAK
CAN_Write(PCAN_USBBUS1, &msg);
```

### Vector CANlib

```c
#include "canlib.h"

// Send via Vector CANlib
canWrite(handle, poco_frame.can_id, poco_frame.data, 
         poco_frame.data_length, canMSG_EXT);
```

## Protocol Details

The Lumitec Poco protocol uses NMEA2000 PGN 61184 with the following format:

| Byte | Description |
|------|-------------|
| 0-1  | Manufacturer Code (1512) + Industry Code (4) |
| 2    | Proprietary ID (message type) |
| 3-7  | Message-specific data |

### CAN ID Format

The 29-bit CAN ID follows NMEA2000 format:
- Bits 28-26: Priority (typically 6)
- Bits 25-8: PGN (61184 = 0xEF00)
- Bits 7-0: Source address

## License

This library is provided by Lumitec for use with Lumitec Poco lighting systems.

## Support

For technical support or questions about the Poco protocol, contact Lumitec technical support.

## Version History

- v1.0.0 - Initial release with core functionality

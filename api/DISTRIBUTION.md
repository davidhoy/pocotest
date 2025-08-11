# Lumitec Poco CAN API Distribution Package

This directory contains the complete Lumitec Poco CAN API library for integration with customer systems.

## Package Contents

```c
api/
├── lumitec_poco_api.h          # Main API header file
├── lumitec_poco_api.c          # API implementation
├── nmea2000_integration.h      # NMEA2000 integration helpers (optional)
├── nmea2000_integration.cpp    # NMEA2000 integration implementation (optional)
├── example_usage.c             # Basic usage example
├── test_poco_api.c             # Comprehensive test suite
├── Makefile                    # Build configuration
└── README.md                   # Documentation
```

## Library Features

✅ **Portable C API** - Works with any CAN stack or raw CAN interface  
✅ **Zero dependencies** - No external library requirements  
✅ **Complete protocol support** - All Poco External Switch commands  
✅ **Comprehensive testing** - 51 automated tests with 100% pass rate  
✅ **Example code** - Ready-to-use integration examples  
✅ **NMEA2000 helpers** - Optional integration with NMEA2000 libraries  

## Quick Integration

1. **Copy files** to your project:

   - `lumitec_poco_api.h`
   - `lumitec_poco_api.c`

2. **Include in your code**:

```c
#include "lumitec_poco_api.h"
```

## **Use the API**

```c
lumitec_poco_can_frame_t frame;
lumitec_poco_create_simple_action(&frame, 0x0E, 0x10, POCO_ACTION_ON, 1);
// Send frame via your CAN interface
```

## Supported CAN Stacks

- Raw SocketCAN (Linux)
- PEAK CAN API
- Vector CANlib  
- Kvaser CANlib
- Any custom CAN implementation

## Testing

Run the test suite to verify functionality:

```bash
make test
```

## Support

The API library provides all functionality needed to implement Lumitec Poco lighting control. For protocol questions or technical support, contact Lumitec.

---

**Lumitec Poco CAN API v1.0.0**  
*Ready for customer distribution*

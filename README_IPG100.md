# IPG100 Support Configuration

The IPG100 support in this application is implemented using conditional compilation, allowing you to easily enable or disable the feature at build time.

## Current Status
IPG100 support is **DISABLED** by default.

## Building with IPG100 Support

### Method 1: Using qmake (Recommended)

#### Disabled (Default)
```bash
qmake && make
```

#### Enabled  
```bash
qmake "CONFIG+=ipg100" && make
```

### Method 2: Using make with CPPFLAGS

#### Disabled (Default)
```bash
make
```

#### Enabled
```bash
make CPPFLAGS="-DENABLE_IPG100_SUPPORT"
```

### Method 3: Modifying the .pro file permanently
You can also modify `pocotest.pro` to include the define permanently:
```qmake
DEFINES += ENABLE_IPG100_SUPPORT
```

## Features Controlled by ENABLE_IPG100_SUPPORT

When IPG100 support is enabled, the following features are available:

1. **IPG100 Header Include**: The NMEA2000_IPG100.h header is included
2. **Add IPG100 Button**: Toolbar button for manually adding IPG100 devices
3. **IPG100 Interface Creation**: Support for creating IPG100 network interfaces
4. **IPG100 Device Discovery**: Automatic discovery of IPG100 devices on the network
5. **IPG100 Interface Verification**: Diagnostic information for IPG100 connections
6. **Manual IPG100 Addition**: Function to manually add IPG100 devices by IP address

When IPG100 support is disabled, these features are compiled out entirely, resulting in a smaller binary and no IPG100 dependencies.

## Code Organization

The conditional compilation uses `#ifdef ENABLE_IPG100_SUPPORT` blocks around:

- **src/devicemainwindow.h**: Function declarations
- **src/devicemainwindow.cpp**: 
  - Header includes
  - UI elements (buttons, etc.)
  - Interface creation logic
  - Device discovery
  - Interface verification
  - Manual device addition function

This approach allows the IPG100 functionality to be completely removed from the build when not needed, while preserving the code for future use.

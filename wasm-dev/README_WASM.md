# WebAssembly (WASM) Build Configuration

This document describes how to build and run the NMEA2000 Analyzer as a WebAssembly application.

## Overview

The WASM build allows the NMEA2000 Analyzer to run in web browsers without requiring native installation. This version uses stub interfaces for NMEA2000 communication and is primarily useful for:

- Demonstration purposes
- UI development and testing
- Cross-platform compatibility testing
- Remote access scenarios (with future WebSocket bridge implementation)

## Prerequisites

### 1. Qt for WebAssembly
Install Qt with WebAssembly support:
- Qt 6.5.0 or later recommended
- Download from: https://www.qt.io/download
- Ensure "Qt for WebAssembly" component is selected during installation

### 2. Emscripten SDK (emsdk)
```bash
# Install emsdk
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

### 3. Qt Creator Configuration
1. Open Qt Creator
2. Go to Tools → Options → Kits
3. Add a new kit for WebAssembly:
   - Name: "Qt WASM"
   - Compiler: Use the emscripten compiler from emsdk
   - Qt version: Select your Qt WASM installation
   - CMake/qmake: Point to Qt WASM tools

## Building

### Method 1: Using the Build Script
```bash
cd /path/to/pocotest
./build-wasm.sh
```

### Method 2: Manual Build
```bash
# Create build directory
mkdir build-wasm && cd build-wasm

# Configure (adjust Qt path as needed)
$HOME/Qt/6.5.0/wasm_singlethread/bin/qmake CONFIG+=wasm CONFIG+=release ../pocotest.pro

# Build
make -j$(nproc)
```

### Method 3: Qt Creator
1. Open `pocotest.pro` in Qt Creator
2. Select the Qt WASM kit
3. Build → Build Project

## Running

After a successful build, you'll have these files:
- `pocotest.html` - Main HTML file
- `pocotest.wasm` - WebAssembly binary
- `pocotest.js` - JavaScript runtime
- `qtloader.js` - Qt WASM loader

### Serve the Application
```bash
cd build-wasm
python3 -m http.server 8080
```

Then open: http://localhost:8080/pocotest.html

**Note:** WASM applications must be served over HTTP/HTTPS, not opened directly as files.

## Current Limitations

### NMEA2000 Communication
- **No real hardware access**: WASM builds use stub interfaces
- **No SocketCAN support**: Browser security prevents direct hardware access
- **IPG100 disabled**: Network interfaces not yet implemented for WASM

### File System
- **Limited file access**: Only files selected by user can be accessed
- **No direct log writing**: Use browser download APIs instead

### Threading
- **Single-threaded**: WASM builds use single-threaded Qt

## Future Enhancements

### WebSocket Bridge Implementation
To enable real NMEA2000 communication in WASM:

1. **Create bridge daemon** (runs on target system):
   ```cpp
   // nmea2000-bridge.cpp
   int main() {
       WebSocketServer wsServer(8080);
       tNMEA2000_SocketCAN can("can0");
       
       // Bridge CAN ↔ WebSocket
       while(true) {
           // Forward CAN messages to WebSocket clients
           // Forward WebSocket messages to CAN bus
       }
   }
   ```

2. **Update WASM interface**:
   ```cpp
   // In NMEA2000_WASM.cpp
   bool tNMEA2000_WASM::CANOpen() {
       // Connect to WebSocket bridge
       return connectToWebSocketBridge();
   }
   ```

3. **Deployment architecture**:
   ```
   Browser (WASM) ↔ WebSocket ↔ Bridge Daemon ↔ CAN Hardware
   ```

## Build Configuration Details

### Conditional Compilation
The project uses these macros for platform-specific code:

- `WASM_BUILD`: Defined for WASM builds
- `ENABLE_IPG100_SUPPORT`: Disabled for WASM builds
- `!wasm`: qmake condition for non-WASM builds

### Source Files
WASM builds use:
- `src/NMEA2000_WASM.cpp` instead of `NMEA2000_SocketCAN.cpp`
- Standard Qt widgets and business logic
- Modified timing functions for WASM compatibility

### Dependencies
WASM builds exclude:
- Linux-specific headers (`linux/can.h`, `sys/socket.h`)
- SocketCAN libraries
- IPG100 TCP networking (for now)

## Troubleshooting

### Build Errors
1. **"emsdk not found"**: Ensure emsdk is installed and activated
2. **"Qt WASM not found"**: Check Qt installation path in build script
3. **"Cannot find qmake"**: Verify Qt WASM installation

### Runtime Errors
1. **"Failed to load WASM"**: Ensure serving over HTTP, not file://
2. **"Module not found"**: Check all files are in same directory
3. **"Qt initialization failed"**: Try different Qt WASM version

### Performance Issues
1. **Slow loading**: WASM files are large, consider compression
2. **UI lag**: Single-threaded WASM may be slower than native

## Files Added/Modified

### New Files
- `src/NMEA2000_WASM.h` - WASM stub interface header
- `src/NMEA2000_WASM.cpp` - WASM stub interface implementation
- `build-wasm.sh` - Build script for WASM
- `README_WASM.md` - This documentation

### Modified Files
- `pocotest.pro` - Added WASM configuration
- `src/devicemainwindow.cpp` - Conditional compilation for WASM
- `src/n2k_linux_port.cpp` - Excluded timing functions for WASM

## Contributing

To extend WASM support:

1. **Add WebSocket communication** for real NMEA2000 data
2. **Implement file upload/download** for log files
3. **Add mobile-friendly UI** adaptations
4. **Create deployment documentation** for various servers

The WASM build provides an excellent foundation for creating a cross-platform, web-based version of the NMEA2000 Analyzer.

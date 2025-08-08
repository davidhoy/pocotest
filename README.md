# NMEA2000 Network Analyzer

A professional Qt-based NMEA2000 network diagnostic tool featuring real-time device discovery, PGN instance conflict detection, and comprehensive network analysis capabilities.

## Features

- **Device-Centric Interface**: Main window focused on discovered NMEA2000 devices
- **Real-Time PGN Instance Tracking**: Monitor and detect instance conflicts across the network
- **Professional Device List**: Manufacturer mapping, installation descriptions, and conflict highlighting
- **Live PGN Log**: Secondary window for detailed message analysis
- **Cross-Platform Support**: Linux, Windows, and macOS compatibility
- **Multiple CAN Interface Support**: SocketCAN (Linux), Peak CAN, Vector, and other Windows drivers

## Prerequisites

- **Qt 5 or 6** (e.g., `qtbase5-dev` or `qt6-base-dev`)
- **CMake** (version 3.10+)
- **C++ Compiler** (e.g., `g++`, `clang`, MSVC)
- **CAN Interface Drivers** (platform-specific)

## Installation

### Linux (Ubuntu/Debian)

```sh
sudo apt update
sudo apt install qtbase5-dev cmake g++ can-utils
```

### Linux (Fedora)

```sh
sudo dnf install qt5-qtbase-devel cmake gcc-c++ can-utils
```

### Windows

#### Option 1: Using vcpkg (Recommended)

```cmd
# Install vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# Install Qt and dependencies
.\vcpkg install qt5-base qt5-widgets
```

#### Option 2: Manual Installation

1. **Install Qt**: Download from [qt.io](https://www.qt.io/download)
2. **Install Visual Studio**: Community Edition with C++ support
3. **Install CAN Driver**: Peak PCAN, Vector, or Kvaser drivers

### macOS (using Homebrew)

```sh
brew install qt cmake
```

## CAN Interface Support

### Linux
- **SocketCAN**: Native Linux CAN support (included)
- **Virtual CAN**: For testing and development

### Windows
The application can be adapted for Windows CAN interfaces:
- **Peak PCAN**: Popular USB-CAN adapters
- **Vector**: Professional CAN interfaces  
- **Kvaser**: Industrial CAN solutions
- **Ixxat**: HMS CAN interfaces

*Note: Windows CAN support requires platform-specific drivers and slight code modifications to replace SocketCAN calls.*

## Building the Project

### Linux

```sh
git clone https://github.com/davidhoy/pocotest.git
cd pocotest
make
```

### Windows (Visual Studio)

```cmd
git clone https://github.com/davidhoy/pocotest.git
cd pocotest
# Open pocotest.pro in Qt Creator or use qmake
qmake
nmake  # or use Visual Studio
```

### Windows (MinGW)

```sh
git clone https://github.com/davidhoy/pocotest.git
cd pocotest
qmake
mingw32-make
```

## Windows CAN Interface Implementation

To adapt this application for Windows CAN interfaces, you would need to:

1. **Replace SocketCAN Backend**: Create a Windows-specific CAN interface class
2. **Add CAN Driver Support**: Integrate with Peak PCAN, Vector, or Kvaser APIs
3. **Update Build System**: Modify the .pro file for Windows-specific libraries

### Example Windows CAN Integration

```cpp
// For Peak PCAN (example)
#ifdef _WIN32
#include "PCANBasic.h"
class tNMEA2000_PCAN : public tNMEA2000 {
    // Implement Windows PCAN interface
};
#endif
```

Popular Windows CAN APIs:
- **Peak PCAN-Basic API**: Simple USB-CAN integration
- **Vector XL Driver Library**: Professional CAN/LIN/FlexRay
- **Kvaser CANlib**: Wide hardware support
- **Ixxat VCI**: HMS industrial interfaces

## Tunneling CAN Traffic with canneloni

To forward CAN traffic from a remote device with CAN hardware to your local machine, you can use [canneloni](https://github.com/mguentner/canneloni).

### Example Setup

#### On the remote device (with CAN hardware)

```sh
canneloni -i can0 -r <local_device_ip> -p 20000
```

- `-i can0`: CAN interface to tunnel.
- `-r <local_device_ip>`: IP address of your local device.
- `-p 20000`: UDP port (default is 20000).

#### On your local device

```sh
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0
canneloni -i vcan0 -l -p 20000
```

- `-i vcan0`: Virtual CAN interface.
- `-l`: Listen mode.
- `-p 20000`: UDP port (must match remote).

Now, CAN traffic from the remote device's `can0` will be available on your local `vcan0` interface.

## Running

After building, run the application:

### Linux
```sh
./pocotest
```

### Windows  
```cmd
pocotest.exe
```

The application will auto-detect available CAN interfaces and display them in the interface dropdown.

## Platform-Specific Notes

### Linux
- Uses SocketCAN for native CAN support
- Requires `can-utils` for CAN interface management
- Supports virtual CAN interfaces for development/testing

### Windows
- Requires CAN driver installation (Peak, Vector, Kvaser, etc.)
- May need administrator privileges for CAN access
- USB-CAN adapters are plug-and-play with proper drivers

### macOS
- Limited native CAN support
- USB-CAN adapters recommended
- May require third-party CAN libraries

## Development Status

This project currently implements:
- ✅ Linux SocketCAN support
- ✅ Real-time NMEA2000 device discovery
- ✅ PGN instance conflict detection
- ✅ Professional device management UI
- 🔄 Windows CAN interface support (planned)
- 🔄 macOS CAN interface support (planned)

## Contributing

Contributions for Windows and macOS CAN interface implementations are welcome!

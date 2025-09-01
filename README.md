# Lumitec Poco Tester

A professional Qt-based NMEA2000 network diagnostic tool featuring real-time device discovery, PGN instance conflict detection, comprehensive network analysis capabilities, and specialized Lumitec Poco protocol support.

## Features

- **Device-Centric Interface**: Main window focused on discovered NMEA2000 devices
- **Real-Time PGN Instance Tracking**: Monitor and detect instance conflicts across the network
- **Professional Device List**: Manufacturer mapping, installation descriptions, and conflict highlighting
- **Live PGN Log**: Secondary window for detailed message analysis with pause/stop/start controls
- **Advanced Signal Extraction**: Proper scaling, offsets, enumerated values, and unit conversion
- **Intelligent Message Naming**: Clean PGN names with proprietary message detection
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

### Windows CAN Interfaces

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
- **Kvaser**: Industrial CAN solutions (using SocketCAN)

## Using Kvaser Leaf CAN Devices on Linux

The Kvaser Leaf family of USB-CAN interfaces is well supported on Linux through the Kvaser drivers and SocketCAN integration. This section covers setting up a Kvaser Leaf v3 or similar device.

### 1. Install Kvaser Drivers

#### Option A: Using Distribution Packages (Recommended)

```sh
# Ubuntu/Debian
sudo apt update
sudo apt install kvaser-drivers-can

# If not available in repos, or for latest version, use Option B
```

#### Option B: Download from Kvaser

1. Download the latest Linux drivers from [Kvaser Downloads](https://www.kvaser.com/download/)
2. Extract and install:

```sh
tar -xzf linuxcan.tar.gz
cd linuxcan
make
sudo make install
```

### 2. Load Kvaser Kernel Modules

```sh
# Load the Kvaser USB driver
sudo modprobe kvaser_usb

# For older devices, you might need:
# sudo modprobe kvaser_usb_leaf
# sudo modprobe kvaser_usb_hydra

# Verify the module is loaded
lsmod | grep kvaser
```

### 3. Connect and Verify Device

```sh
# Connect your Kvaser Leaf device and check if it's detected
dmesg | tail -10

# You should see something like:
# kvaser_usb 1-1:1.0: Kvaser CAN device connected
# can: controller area network core (rev 20120528 ack)
# kvaser_usb 1-1:1.0 can0: registered kvaser_usb device

# List CAN interfaces
ip link show type can
```

### 4. Configure and Bring Up the Interface

```sh
# Set CAN bitrate (250kbps is standard for NMEA2000)
sudo ip link set can0 type can bitrate 250000

# Optional: Set other CAN parameters
# sudo ip link set can0 type can bitrate 250000 sample-point 0.875 restart-ms 1000

# Bring the interface up
sudo ip link set can0 up

# Verify interface is active
ip link show can0
# Should show: can0: <NOARP,UP,LOWER_UP> mtu 16 qdisc pfifo_fast state UP
```

### 5. Test the Interface

```sh
# Monitor CAN traffic (install can-utils if not present)
sudo apt install can-utils
candump can0

# In another terminal, send a test frame
cansend can0 123#DEADBEEF

# You should see the frame in candump output
```

### 6. Run the Application

```sh
# Start the NMEA2000 analyzer
./pocotest

# The application should automatically detect can0 in the interface dropdown
# If can0 is the first available interface, it will be selected by default
```

### 7. Troubleshooting

#### Device Not Detected

```sh
# Check USB connection
lsusb | grep Kvaser

# Check kernel messages
dmesg | grep -i kvaser

# Manually load driver
sudo modprobe kvaser_usb
```

#### Permission Issues

```sh
# Add user to dialout group for CAN access
sudo usermod -a -G dialout $USER

# Or create udev rule for CAN devices
echo 'KERNEL=="can*", GROUP="dialout", MODE="0664"' | sudo tee /etc/udev/rules.d/99-can.rules
sudo udevadm control --reload-rules
```

#### Interface Won't Come Up

```sh
# Check for conflicting processes
sudo lsof /dev/can0

# Reset the interface
sudo ip link set can0 down
sudo ip link set can0 type can bitrate 250000
sudo ip link set can0 up
```

### 8. Automatic Setup Script

Create a script to automate the setup process:

```sh
#!/bin/bash
# kvaser_setup.sh

echo "Setting up Kvaser Leaf CAN interface..."

# Load driver
sudo modprobe kvaser_usb

# Wait for device detection
sleep 2

# Configure interface
sudo ip link set can0 down 2>/dev/null || true
sudo ip link set can0 type can bitrate 250000
sudo ip link set can0 up

echo "Kvaser setup complete. Interface status:"
ip link show can0
```

Make it executable: `chmod +x kvaser_setup.sh`

### 9. Persistent Configuration

To automatically configure the interface on boot, add to `/etc/network/interfaces`:

```sh
# Add to /etc/network/interfaces
auto can0
iface can0 can static
    bitrate 250000
    pre-up /sbin/modprobe kvaser_usb
```

Or create a systemd service:

```ini
# /etc/systemd/system/kvaser-can.service
[Unit]
Description=Kvaser CAN Interface Setup
After=network.target

[Service]
Type=oneshot
ExecStart=/bin/bash -c 'modprobe kvaser_usb && sleep 2 && ip link set can0 type can bitrate 250000 && ip link set can0 up'
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
```

Enable with: `sudo systemctl enable kvaser-can.service`

### Windows

The application can be adapted for Windows CAN interfaces:

- **Peak PCAN**: Popular USB-CAN adapters
- **Vector**: Professional CAN interfaces  
- **Kvaser**: Industrial CAN solutions
- **Ixxat**: HMS CAN interfaces

*Note: Windows CAN support requires platform-specific drivers and slight code modifications to replace SocketCAN calls.*

## Building the Project

### Linux (Desktop)

```sh
git clone --recurse-submodules https://github.com/davidhoy/pocotest.git
cd pocotest
qmake
make
```

## ⚠️ Note for VS Code Users

If you are using **VS Code** on Linux, **do not** use the Snap build of VS Code to build or run this project.  
The Snap version injects Snap base library paths (e.g., `/snap/core20/...`) into the integrated terminal environment,  
which can cause the app to link against the wrong version of `glibc`/`libpthread` and fail at runtime with errors like:

### How to Avoid the Issue

- **Recommended:** Install the official `.deb` package from Microsoft’s repository instead of the Snap:

  ```bash
  sudo snap remove code
  wget -qO- https://packages.microsoft.com/keys/microsoft.asc \
    | gpg --dearmor \
    | sudo tee /usr/share/keyrings/ms_vscode.gpg >/dev/null
  echo "deb [arch=amd64,arm64 signed-by=/usr/share/keyrings/ms_vscode.gpg] \
    https://packages.microsoft.com/repos/code stable main" \
    | sudo tee /etc/apt/sources.list.d/vscode.list
  sudo apt update
  sudo apt install code


### Raspberry Pi (Native Build - Recommended)

Building directly on the Raspberry Pi is the **recommended approach** because:

- ✅ **Perfect compatibility** - uses exact Pi libraries and ABI
- ✅ **No cross-compilation complexity** - avoids toolchain issues  
- ✅ **Optimal performance** - compiler optimizes for actual hardware
- ✅ **Easy debugging** - native development tools available
- ✅ **Reliable builds** - standard package management

#### 1. Install Dependencies

```sh
sudo apt update
sudo apt install -y build-essential cmake qtbase5-dev qt5-default libqt5widgets5 libqt5gui5 libqt5core5a can-utils git
```

#### 2. Clone and Build

```sh
git clone --recurse-submodules https://github.com/davidhoy/pocotest.git
cd pocotest

# Option A: Using CMake (recommended)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Option B: Using qmake
# cd pocotest  # if using CMake, go back to project root
qmake pocotest.pro
make -j$(nproc)
```

#### 3. Setup CAN Interface

```sh
# If you have CAN hardware (e.g., MCP2515, CAN HAT), use can0 directly:
sudo ip link set can0 up type can bitrate 250000

# For testing without hardware, use virtual CAN:
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0

# For MCP2515 CAN controller, add to /boot/config.txt:
# dtparam=spi=on
# dtoverlay=mcp2515-can0,oscillator=16000000,interrupt=25
# Then reboot and use the can0 command above
```

#### 4. Run

```sh
./pocotest  # or ./build/pocotest if using CMake
```

### Windows (Visual Studio)

```sh
git clone --recurse-submodules https://github.com/davidhoy/pocotest.git
cd pocotest
# Open pocotest.pro in Qt Creator or use qmake
qmake
nmake  # or use Visual Studio
```

### Windows (MinGW)

```sh
git clone --recurse-submodules https://github.com/davidhoy/pocotest.git
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

## Using Maretron IPG100 as Network CAN Interface

The Maretron IPG100 is a professional NMEA 2000 to Ethernet gateway that can serve as an excellent network-based CAN interface. It provides multiple ways to access NMEA 2000 data over your network.

### IPG100 Overview

The IPG100 offers several network protocols for NMEA 2000 access:

- **NMEA 0183 over TCP/UDP** - Converted NMEA 2000 messages
- **Raw NMEA 2000 over TCP** - Direct binary PGN access
- **Actisense NGT-1 protocol emulation** - Compatible with many tools
- **Web interface** - Configuration and monitoring

### Method 1: Using Actisense NGT-1 Emulation (Recommended)

The IPG100 can emulate an Actisense NGT-1 interface over TCP, which many NMEA 2000 tools support.

#### 1. Configure IPG100

1. Access the IPG100 web interface: `http://<ipg100_ip>`
2. Go to **Services** → **Actisense NGT-1**
3. Enable the service on port 2597 (default)
4. Set output format to "Raw NMEA 2000"

#### 2. Create TCP-to-CAN Bridge

You'll need a bridge application to convert TCP data to SocketCAN. Here's a Python example:

```python
#!/usr/bin/env python3
# ipg100_bridge.py
import socket
import struct
import subprocess
import sys

IPG100_IP = "192.168.1.100"  # Replace with your IPG100 IP
IPG100_PORT = 2597
VCAN_INTERFACE = "vcan1"

def setup_vcan():
    """Setup virtual CAN interface"""
    subprocess.run(["sudo", "modprobe", "vcan"], check=True)
    subprocess.run(["sudo", "ip", "link", "add", "dev", VCAN_INTERFACE, "type", "vcan"], check=False)
    subprocess.run(["sudo", "ip", "link", "set", "up", VCAN_INTERFACE], check=True)

def actisense_to_can():
    """Bridge Actisense NGT-1 data to SocketCAN"""
    setup_vcan()
    
    # Connect to IPG100
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((IPG100_IP, IPG100_PORT))
    
    print(f"Connected to IPG100 at {IPG100_IP}:{IPG100_PORT}")
    print(f"Bridging to {VCAN_INTERFACE}")
    
    # Open CAN socket
    can_sock = socket.socket(socket.PF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
    can_sock.bind((VCAN_INTERFACE,))
    
    buffer = b""
    
    while True:
        try:
            data = sock.recv(1024)
            if not data:
                break
                
            buffer += data
            
            # Process Actisense frames
            while len(buffer) >= 3:
                if buffer[0:2] != b'\x10\x02':  # Actisense start sequence
                    buffer = buffer[1:]
                    continue
                    
                if len(buffer) < 3:
                    break
                    
                msg_len = buffer[2]
                if len(buffer) < msg_len + 3:
                    break
                    
                # Extract and convert to CAN frame
                frame_data = buffer[3:3+msg_len]
                buffer = buffer[3+msg_len:]
                
                if len(frame_data) >= 8:  # Valid N2K frame
                    # Convert to SocketCAN frame format
                    can_id = struct.unpack('<I', frame_data[0:4])[0]
                    can_data = frame_data[4:4+8]
                    
                    # Send to CAN interface
                    can_frame = struct.pack('<IB3x8s', can_id, len(can_data), can_data)
                    can_sock.send(can_frame)
                    
        except KeyboardInterrupt:
            break
        except Exception as e:
            print(f"Error: {e}")
            break
    
    sock.close()
    can_sock.close()

if __name__ == "__main__":
    actisense_to_can()
```

#### 3. Run the Bridge

```sh
# Make the script executable
chmod +x ipg100_bridge.py

# Install dependencies
pip3 install socket struct

# Run the bridge
python3 ipg100_bridge.py
```

#### 4. Use with Your Application

```sh
# The bridge creates vcan1, use it in your app
./pocotest -i vcan1

# Or select vcan1 from the interface dropdown
```

### Method 2: Direct TCP Connection (Advanced)

For more direct integration, you could modify the NMEA2000 library to connect directly to the IPG100:

```cpp
// Example TCP NMEA2000 interface class
class tNMEA2000_IPG100 : public tNMEA2000 {
private:
    int tcp_socket;
    std::string ipg100_ip;
    int ipg100_port;
    
public:
    tNMEA2000_IPG100(const char* ip, int port = 2597) 
        : ipg100_ip(ip), ipg100_port(port), tcp_socket(-1) {}
    
    bool CANOpen() override {
        // Connect to IPG100 TCP port
        tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(ipg100_port);
        inet_pton(AF_INET, ipg100_ip.c_str(), &addr.sin_addr);
        
        return connect(tcp_socket, (struct sockaddr*)&addr, sizeof(addr)) == 0;
    }
    
    bool CANGetFrame(unsigned long &id, unsigned char &len, unsigned char *buf) override {
        // Receive and parse Actisense/raw data from TCP
        // Convert to NMEA2000 frame format
        // Implementation depends on IPG100 output format
    }
    
    bool CANSendFrame(unsigned long id, unsigned char len, const unsigned char *buf, bool wait_sent) override {
        // Convert NMEA2000 frame to IPG100 format and send via TCP
        // Note: IPG100 may be read-only depending on configuration
    }
};
```

### Method 3: Using n2kd (NMEA 2000 Daemon)

You can use `n2kd` from the canboat project as an intermediary:

```sh
# Install canboat tools
git clone https://github.com/canboat/canboat.git
cd canboat
make
sudo make install

# Configure n2kd for IPG100
# Create config file pointing to IPG100 Actisense port
echo "interface=tcp:192.168.1.100:2597" > /etc/n2kd.conf

# Run n2kd with SocketCAN output
n2kd -d vcan2

# Use vcan2 in your application
./pocotest -i vcan2
```

### IPG100 Configuration Tips

#### Network Setup
1. **Static IP**: Assign a static IP to the IPG100 for reliable connections
2. **VLAN**: Consider placing on a dedicated marine network VLAN
3. **Firewall**: Open required ports (2597 for Actisense, 80 for web interface)

#### Service Configuration
1. **Enable Raw NMEA 2000**: For best compatibility with analysis tools
2. **Set Update Rate**: Configure for your analysis needs (1-10 Hz typical)
3. **Message Filtering**: Enable all PGNs or filter for specific messages

#### Performance Considerations
- **Network Latency**: Ethernet adds ~1-5ms vs direct CAN
- **Bandwidth**: IPG100 can handle full NMEA 2000 bus traffic
- **Reliability**: More robust than USB-CAN in marine environments

### Advantages of IPG100 Interface

✅ **Network-based**: Access from anywhere on your network  
✅ **Professional grade**: Marine-certified, robust hardware  
✅ **Multiple protocols**: Supports various connection methods  
✅ **Web management**: Easy configuration and monitoring  
✅ **Galvanic isolation**: Protects your computer from ground loops  
✅ **Multiple clients**: Can serve data to multiple applications  

### Disadvantages

❌ **Network dependency**: Requires stable Ethernet connection  
❌ **Added latency**: Small delay vs direct CAN interface  
❌ **Complexity**: Requires bridge software or custom integration  
❌ **Cost**: More expensive than basic USB-CAN adapters  

### Troubleshooting IPG100 Connection

```sh
# Test IPG100 network connectivity
ping <ipg100_ip>

# Test Actisense port
telnet <ipg100_ip> 2597

# Check IPG100 web interface
curl http://<ipg100_ip>/status

# Monitor bridge traffic
candump vcan1

# Verify IPG100 is receiving NMEA 2000 data
# Check web interface statistics page
```

## Running

After building, run the application:

### Linux (Running)

```sh
./pocotest
```

### Windows (Running)

```cmd
pocotest.exe
```

The application will auto-detect available CAN interfaces and display them in the interface dropdown.

## Platform-Specific Notes

### Linux (Notes)

- Uses SocketCAN for native CAN support
- Requires `can-utils` for CAN interface management
- Supports virtual CAN interfaces for development/testing

### Windows (Notes)

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
- ✅ Professional signal extraction and unit conversion
- ✅ Interactive PGN log with filtering and pause controls
- 🔄 Windows CAN interface support (planned)
- 🔄 macOS CAN interface support (planned)

### DBC File Support

The application includes a comprehensive NMEA2000 DBC file with 255+ message definitions sourced from the canboat project, providing professional-grade marine protocol support.

## Contributing

Contributions for Windows and macOS CAN interface implementations are welcome!

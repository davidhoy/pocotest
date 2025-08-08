# pocotest

A sample project demonstrating usage of QT and other dependencies.

## Prerequisites

- **Qt 5 or 6** (e.g., `qtbase5-dev` or `qt6-base-dev`)
- **CMake** (version 3.10+)
- **C++ Compiler** (e.g., `g++`, `clang`)
- **Poco Libraries** (e.g., `libpoco-dev`)

## Installation

### Ubuntu/Debian

```sh
sudo apt update
sudo apt install qtbase5-dev cmake g++ libpoco-dev
```

### Fedora

```sh
sudo dnf install qt5-qtbase-devel cmake gcc-c++ poco-devel
```

### macOS (using Homebrew)

```sh
brew install qt cmake poco
```

## Building the Project

```sh
git clone https://github.com/davidhoy/pocotest.git
cd pocotest
mkdir build
cd build
cmake ..
make
```

## Tunneling CAN Traffic with canneloni

To forward CAN traffic from a remote device with CAN hardware to your local machine, you can use [canneloni](https://github.com/mguentner/canneloni).

### Example Setup

#### On the remote device (with CAN hardware):

```sh
canneloni -i can0 -r <local_device_ip> -p 20000
```

- `-i can0`: CAN interface to tunnel.
- `-r <local_device_ip>`: IP address of your local device.
- `-p 20000`: UDP port (default is 20000).

#### On your local device:

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

After building, the executable will be in the `build` directory.  The application will autodetect the available CAN interfaces:

```sh
./pocotest
```

## Notes

- Ensure Qt and Poco libraries are discoverable by your compiler and linker.
- For custom Qt versions, set `CMAKE_PREFIX_PATH` accordingly.

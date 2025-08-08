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

## Running

After building, the executable will be in the `build` directory:

```sh
./pocotest
```

## Notes

- Ensure Qt and Poco libraries are discoverable by your compiler and linker.
- For custom Qt versions, set `CMAKE_PREFIX_PATH` accordingly.

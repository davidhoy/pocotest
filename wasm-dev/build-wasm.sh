#!/bin/bash

# Build script for Qt WASM version of NMEA2000 Analyzer
# Prerequisites:
# 1. Qt for WebAssembly installed (Qt 6.5+ recommended)
# 2. emsdk installed and activated
# 3. Qt WASM kit configured in Qt Creator

echo "Building NMEA2000 Analyzer for WebAssembly..."

# Check if we're in the right directory
if [ ! -f "pocotest.pro" ]; then
    echo "Error: Run this script from the project root directory"
    exit 1
fi

# Clean previous WASM build
echo "Cleaning previous WASM build..."
rm -rf build-wasm/
mkdir -p build-wasm
cd build-wasm

# Find Qt WASM qmake (adjust path as needed)
# First check if we have emscripten
if ! command -v emcc &> /dev/null; then
    echo "Error: Emscripten (emcc) not found!"
    echo "Please install Emscripten first:"
    echo "  git clone https://github.com/emscripten-core/emsdk.git"
    echo "  cd emsdk"
    echo "  ./emsdk install latest"
    echo "  ./emsdk activate latest"
    echo "  source ./emsdk_env.sh"
    echo ""
    exit 1
fi

# Try different possible Qt WASM locations
QMAKE_WASM=""
if [ -f "/usr/lib/qt5/bin/qmake" ]; then
    # Check if this Qt supports WASM
    if [ -d "/usr/lib/x86_64-linux-gnu/qt5/mkspecs/wasm-emscripten" ]; then
        echo "Found Qt5 with WASM support, checking if it works..."
        QMAKE_WASM="/usr/lib/qt5/bin/qmake"
    fi
elif [ -f "$HOME/Qt/6.5.0/wasm_singlethread/bin/qmake" ]; then
    QMAKE_WASM="$HOME/Qt/6.5.0/wasm_singlethread/bin/qmake"
elif [ -f "$HOME/Qt/6.6.0/wasm_singlethread/bin/qmake" ]; then
    QMAKE_WASM="$HOME/Qt/6.6.0/wasm_singlethread/bin/qmake"
elif [ -f "$HOME/Qt/6.7.0/wasm_singlethread/bin/qmake" ]; then
    QMAKE_WASM="$HOME/Qt/6.7.0/wasm_singlethread/bin/qmake"
else
    echo "Qt WASM qmake not found. Please install Qt for WebAssembly."
    echo "Options:"
    echo "1. Install Qt with WebAssembly support from: https://www.qt.io/download"
    echo "2. Or try using system Qt5 with manual WASM setup"
    echo ""
    echo "Current system has Qt $(qmake -query QT_VERSION) at $(qmake -query QT_INSTALL_BINS)"
    if [ -d "/usr/lib/x86_64-linux-gnu/qt5/mkspecs/wasm-emscripten" ]; then
        echo "WASM mkspecs found - you might be able to use system Qt with proper setup"
    fi
    exit 1
fi

echo "Using Qt WASM qmake: $QMAKE_WASM"

# Configure for WASM build
echo "Configuring project for WASM..."
if [[ "$QMAKE_WASM" == "/usr/lib/qt5/bin/qmake" ]]; then
    # Using system Qt5 - need to specify WASM platform explicitly
    $QMAKE_WASM -spec wasm-emscripten CONFIG+=wasm CONFIG-=debug CONFIG+=release ../pocotest.pro
else
    # Using dedicated Qt WASM installation
    $QMAKE_WASM CONFIG+=wasm CONFIG-=debug CONFIG+=release ../pocotest.pro
fi

if [ $? -ne 0 ]; then
    echo "Error: qmake configuration failed"
    exit 1
fi

# Build the project
echo "Building project..."
make -j$(nproc)

if [ $? -eq 0 ]; then
    echo ""
    echo "Build successful!"
    echo "Output files:"
    echo "  HTML: $(pwd)/pocotest.html"
    echo "  WASM: $(pwd)/pocotest.wasm"
    echo "  JS:   $(pwd)/pocotest.js"
    echo ""
    echo "To serve the application:"
    echo "  cd build-wasm"
    echo "  python3 -m http.server 8080"
    echo "  Open: http://localhost:8080/pocotest.html"
    echo ""
    echo "Note: This WASM version uses stub interfaces and won't connect to real hardware."
    echo "For actual NMEA2000 communication, you'll need to implement WebSocket bridging."
else
    echo "Build failed!"
    exit 1
fi

#!/bin/bash

# Manual WASM build script with better compatibility
# This bypasses Qt's problematic WASM configuration

echo "Building NMEA2000 Analyzer for WebAssembly (Manual Mode)..."

# Check if we're in the right directory
if [ ! -f "pocotest.pro" ]; then
    echo "Error: Run this script from the project root directory"
    exit 1
fi

# Check if emcc is available
if ! command -v emcc &> /dev/null; then
    echo "Error: Emscripten (emcc) not found!"
    echo "Please run: source ~/emsdk/emsdk_env.sh"
    exit 1
fi

# Clean previous build
echo "Cleaning previous WASM build..."
rm -rf build-manual-wasm/
mkdir -p build-manual-wasm
cd build-manual-wasm

echo "Creating simple qmake project without threading..."

# Create a simplified .pro file for WASM
cat > simple-wasm.pro << 'EOF'
QT += core gui widgets network

CONFIG += c++11 release
CONFIG -= debug thread

DEFINES += WASM_BUILD QT_NO_DEBUG

TARGET = pocotest
TEMPLATE = app

INCLUDEPATH += \
    ../src \
    ../components/external/NMEA2000/src

SOURCES += \
    ../src/main.cpp \
    ../src/devicemainwindow.cpp \
    ../src/pgnlogdialog.cpp \
    ../src/pgndialog.cpp \
    ../src/pocodevicedialog.cpp \
    ../src/zonelightingdialog.cpp \
    ../src/LumitecPoco.cpp \
    ../src/dbcdecoder.cpp \
    ../src/instanceconflictanalyzer.cpp \
    ../src/NMEA2000_WASM.cpp \
    ../components/external/NMEA2000/src/NMEA2000.cpp \
    ../components/external/NMEA2000/src/N2kTimer.cpp \
    ../components/external/NMEA2000/src/N2kMsg.cpp \
    ../components/external/NMEA2000/src/N2kMessages.cpp \
    ../components/external/NMEA2000/src/N2kStream.cpp \
    ../components/external/NMEA2000/src/N2kGroupFunction.cpp \
    ../components/external/NMEA2000/src/N2kGroupFunctionDefaultHandlers.cpp \
    ../components/external/NMEA2000/src/N2kDeviceList.cpp

HEADERS += \
    ../src/devicemainwindow.h \
    ../src/pgnlogdialog.h \
    ../src/pgndialog.h \
    ../src/pocodevicedialog.h \
    ../src/zonelightingdialog.h \
    ../src/LumitecPoco.h \
    ../src/dbcdecoder.h \
    ../src/instanceconflictanalyzer.h \
    ../src/NMEA2000_WASM.h

RESOURCES += \
    ../resources/resources.qrc
EOF

echo "Configuring simplified WASM project..."
qmake -spec wasm-emscripten simple-wasm.pro

if [ $? -ne 0 ]; then
    echo "Error: qmake configuration failed"
    exit 1
fi

echo "Building project..."
make -j$(nproc) 2>&1 | grep -v "linker setting ignored during compilation" || true

if [ -f "pocotest.html" ]; then
    echo ""
    echo "Build successful!"
    echo "Output files:"
    echo "  HTML: $(pwd)/pocotest.html"
    echo "  WASM: $(pwd)/pocotest.wasm"
    echo "  JS:   $(pwd)/pocotest.js"
    echo ""
    echo "To serve the application:"
    echo "  cd build-manual-wasm"
    echo "  python3 -m http.server 8080"
    echo "  Open: http://localhost:8080/pocotest.html"
    echo ""
    echo "Note: This WASM version uses stub interfaces and won't connect to real hardware."
else
    echo "Build failed - no HTML output generated"
    exit 1
fi

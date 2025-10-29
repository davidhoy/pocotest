QT += core gui widgets network

CONFIG += c++17

# Enable ccache for faster compilation if available
QMAKE_CXX = ccache $$QMAKE_CXX
QMAKE_CC = ccache $$QMAKE_CC

# Platform-specific configuration
wasm {
    message("Building for WebAssembly")
    DEFINES += WASM_BUILD
    # Disable console for WASM builds
    
    # Work around Qt 5.15 + newer Emscripten compatibility issues
    QMAKE_CXXFLAGS += -Wno-unused-command-line-argument
    QMAKE_LFLAGS += -Wno-unused-command-line-argument
    
    # Disable problematic warnings for WASM build
    QMAKE_CXXFLAGS += -Wno-vla-cxx-extension -Wno-character-conversion -Wno-deprecated-literal-operator -Wno-unused-parameter -Wno-unused-lambda-capture
    
    # Work around QScopeGuard template deduction issue with Emscripten
    DEFINES += QT_NO_TEMPLATE_DEDUCTION_GUIDES
    
    # Work around wasm-opt optimization issues with Emscripten 3.1.70
    QMAKE_LFLAGS += -s WASM_BIGINT=0
    
    # Use local HTML template
    exists(wasm-dev/wasm_shell.html): QMAKE_LFLAGS += --shell-file $$PWD/wasm-dev/wasm_shell.html
    
    # Disable threading for simpler WASM build
    CONFIG -= thread
} else {
    CONFIG += console
}

# IPG100 Support Configuration
# To enable IPG100 support, build with: qmake "CONFIG+=ipg100" && make
# To disable (default), build with: qmake && make
ipg100:!wasm {
    message("Building with IPG100 support enabled")
    DEFINES += ENABLE_IPG100_SUPPORT
} else {
    message("Building with IPG100 support disabled (default)")
}

# Treat warnings as errors for high code quality (except for WASM)
!wasm {
    QMAKE_CXXFLAGS += -Werror
}

DESTDIR = ./
OBJECTS_DIR = build/obj
MOC_DIR = build/moc
UI_DIR = build/ui
RCC_DIR = build/rcc

# App details
TARGET = pocotest
TEMPLATE = app

# Include paths
INCLUDEPATH += \
    $$PWD/src \
    $$PWD/components/external/NMEA2000/src

# Platform-specific include paths
!wasm {
    INCLUDEPATH += $$PWD/components/external/NMEA2000_socketCAN
}

# Conditionally include IPG100 paths (disabled for WASM)
ipg100:!wasm {
    INCLUDEPATH += $$PWD/components/external/NMEA2000_IPG100
}

# Main application sources and headers
SOURCES += \
    src/main.cpp \
    src/devicemainwindow.cpp \
    src/pgnlogdialog.cpp \
    src/pgndialog.cpp \
    src/pocodevicedialog.cpp \
    src/zonelightingdialog.cpp \
    src/LumitecPoco.cpp \
    src/n2k_linux_port.cpp \
    src/dbcdecoder.cpp \
    src/instanceconflictanalyzer.cpp \
    src/toastnotification.cpp \
    src/toastmanager.cpp \
    src/thememanager.cpp \
    components/external/NMEA2000/src/NMEA2000.cpp \
    components/external/NMEA2000/src/N2kTimer.cpp \
    components/external/NMEA2000/src/N2kMsg.cpp \
    components/external/NMEA2000/src/N2kMessages.cpp \
    components/external/NMEA2000/src/N2kStream.cpp \
    components/external/NMEA2000/src/N2kGroupFunction.cpp \
    components/external/NMEA2000/src/N2kGroupFunctionDefaultHandlers.cpp \
    components/external/NMEA2000/src/N2kDeviceList.cpp

# Platform-specific sources
!wasm {
    SOURCES += components/external/NMEA2000_socketCAN/NMEA2000_SocketCAN.cpp
} else {
    SOURCES += wasm-dev/NMEA2000_WASM.cpp
}

# Conditionally include IPG100 sources (disabled for WASM)
ipg100:!wasm {
    SOURCES += components/external/NMEA2000_IPG100/NMEA2000_IPG100.cpp
}

HEADERS += \
    src/devicemainwindow.h \
    src/pgnlogdialog.h \
    src/pgndialog.h \
    src/pocodevicedialog.h \
    src/zonelightingdialog.h \
    src/LumitecPoco.h \
    src/dbcdecoder.h \
    src/instanceconflictanalyzer.h \
    src/toastnotification.h \
    src/toastmanager.h \
    src/thememanager.h

# Platform-specific headers
wasm {
    HEADERS += wasm-dev/NMEA2000_WASM.h
}

# Conditionally include IPG100 headers (disabled for WASM)
ipg100:!wasm {
    HEADERS += components/external/NMEA2000_IPG100/NMEA2000_IPG100.h
}

# Resources
RESOURCES += \
    resources/resources.qrc

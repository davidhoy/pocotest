QT += core gui widgets network

CONFIG += c++11 console

# IPG100 Support Configuration
# To enable IPG100 support, build with: qmake "CONFIG+=ipg100" && make
# To disable (default), build with: qmake && make
ipg100 {
    message("Building with IPG100 support enabled")
    DEFINES += ENABLE_IPG100_SUPPORT
} else {
    message("Building with IPG100 support disabled (default)")
}

# Treat warnings as errors for high code quality
QMAKE_CXXFLAGS += -Werror

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
    $$PWD/components/external/NMEA2000/src \
    $$PWD/components/external/NMEA2000_socketCAN

# Conditionally include IPG100 paths
ipg100 {
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
    components/external/NMEA2000/src/NMEA2000.cpp \
    components/external/NMEA2000/src/N2kTimer.cpp \
    components/external/NMEA2000/src/N2kMsg.cpp \
    components/external/NMEA2000/src/N2kMessages.cpp \
    components/external/NMEA2000/src/N2kStream.cpp \
    components/external/NMEA2000/src/N2kGroupFunction.cpp \
    components/external/NMEA2000/src/N2kGroupFunctionDefaultHandlers.cpp \
    components/external/NMEA2000/src/N2kDeviceList.cpp \
    components/external/NMEA2000_socketCAN/NMEA2000_SocketCAN.cpp

# Conditionally include IPG100 sources
ipg100 {
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
    src/instanceconflictanalyzer.h

# Conditionally include IPG100 headers
ipg100 {
    HEADERS += components/external/NMEA2000_IPG100/NMEA2000_IPG100.h
}

# Resources
RESOURCES += \
    resources/resources.qrc

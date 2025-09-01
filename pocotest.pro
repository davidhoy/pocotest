QT += core gui widgets network

CONFIG += c++11 console

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
    $$PWD/components/external/NMEA2000_socketCAN \
    $$PWD/components/external/NMEA2000_IPG100

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
    components/external/NMEA2000_IPG100/NMEA2000_IPG100.cpp \
    components/external/NMEA2000_socketCAN/NMEA2000_SocketCAN.cpp

HEADERS += \
    src/devicemainwindow.h \
    src/pgnlogdialog.h \
    src/pgndialog.h \
    src/pocodevicedialog.h \
    src/zonelightingdialog.h \
    src/LumitecPoco.h \
    src/dbcdecoder.h \
    src/instanceconflictanalyzer.h \
    components/external/NMEA2000_IPG100/NMEA2000_IPG100.h

# Resources
RESOURCES += \
    resources/resources.qrc

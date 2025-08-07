QT += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11 console

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

# Main application sources and headers
SOURCES += \
    components/external/NMEA2000/src/NMEA2000.cpp \
    components/external/NMEA2000/src/N2kTimer.cpp \
    components/external/NMEA2000/src/N2kMsg.cpp \
    components/external/NMEA2000/src/N2kMessages.cpp \
    components/external/NMEA2000/src/N2kStream.cpp \
    components/external/NMEA2000/src/N2kGroupFunction.cpp \
    components/external/NMEA2000/src/N2kGroupFunctionDefaultHandlers.cpp \
    components/external/NMEA2000_socketCAN/NMEA2000_SocketCAN.cpp \
    src/n2k_linux_port.cpp \
    src/main.cpp \
    src/mainwindow.cpp \
    src/pgndialog.cpp \
    src/devicelistdialog.cpp

HEADERS += \
    src/mainwindow.h \
    src/pgndialog.h \
    src/devicelistdialog.h

FORMS += \
    ui/mainwindow.ui

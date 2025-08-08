/**
 * @file main.cpp
 * @brief Entry point for the Lumitec Poco Tester application.
 *
 * This file contains the main function which initializes the Qt application,
 * processes command line arguments for CAN interface selection, and launches
 * the main window of the application.
 */

#include <QApplication>
#include <QDebug>
#include <QIcon>
#include "devicemainwindow.h"

char can_interface[80] = "vcan0";  // Default CAN interface, can be overridden by command line argument


/**
 * @brief Main entry point for the application.
 *
 * Parses command line arguments to set the CAN interface if provided in the form "-i <interface>".
 * Initializes the Qt application and displays the main window.
 *
 * @param argc Number of command line arguments.
 * @param argv Array of command line argument strings.
 * @return Application exit status code.
 */
int main(int argc, char *argv[])
{
    // Set the CAN interface from command line arguments if provided in the form "-i <interface>"
    if (argc > 2 && QString(argv[1]) == "-i") {
        strncpy(can_interface, argv[2], sizeof(can_interface) - 1);
        can_interface[sizeof(can_interface) - 1] = '\0';  // Ensure null-termination
    }

    // Initialize the Qt application
    QApplication app(argc, argv);

    // Set application properties BEFORE setting icons
    QApplication::setApplicationName("NMEA 2000 Analyzer");  // Match desktop file exactly
    QApplication::setApplicationVersion("1.0");
    QApplication::setApplicationDisplayName("NMEA 2000 Analyzer");
    QApplication::setOrganizationName("Lumitec");
    QApplication::setOrganizationDomain("lumitec.com");
    
    // Set the desktop file name for better integration
    QApplication::setDesktopFileName("nmea2000-analyzer.desktop");
    
    // Set the application icon globally
    QIcon appIcon;
    appIcon.addFile(":/app_icon.png", QSize(), QIcon::Normal, QIcon::Off);
    appIcon.addFile(":/app_icon.svg", QSize(), QIcon::Normal, QIcon::Off);
    
    if (appIcon.isNull()) {
        qDebug() << "Warning: Application icon could not be loaded!";
    } else {
        qDebug() << "Application icon loaded successfully (PNG:" << !QIcon(":/app_icon.png").isNull() << "SVG:" << !QIcon(":/app_icon.svg").isNull() << ")";
        QApplication::setWindowIcon(appIcon);
    }
    
    qDebug() << "Creating DeviceMainWindow...";
    DeviceMainWindow w;
    qDebug() << "Showing DeviceMainWindow...";
    w.show();
    qDebug() << "DeviceMainWindow shown, starting event loop...";

    return app.exec();
}

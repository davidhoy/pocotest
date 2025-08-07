/**
 * @file main.cpp
 * @brief Entry point for the Lumitec Poco Tester application.
 *
 * This file contains the main function which initializes the Qt application,
 * processes command line arguments for CAN interface selection, and launches
 * the main window of the application.
 */

#include <QApplication>
#include "mainwindow.h"

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
    //QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    //QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    //QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    //QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
    QApplication app(argc, argv);

    // Create and show the main window
    QApplication::setApplicationName("Lumitec Poco Tester");
    MainWindow w;
    w.show();

    return app.exec();
}

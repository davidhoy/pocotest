#include <QApplication>
#include "mainwindow.h"


char can_interface[80] = "vcan0";  // Default CAN interface, can be overridden by command line argument


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

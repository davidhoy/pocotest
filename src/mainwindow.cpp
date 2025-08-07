/**
 * @file mainwindow.cpp
 * @brief Implementation of the MainWindow class for tMainWindow::~MainWindow() 
{
    delete ui;
    if (nmea2000 != nullptr) {
        // Gracefully shutdown the NMEA 2000 stack
        // Note: NMEA2000 library doesn't have a Close() method
        delete nmea2000;
        nmea2000 = nullptr;
    }
}00 CAN interface application.
 *
 * This file contains the logic for initializing and handling the NMEA2000 communication,
 * managing the main window UI, and processing incoming CAN messages.
 *
 * @author David Hoy
 * @date 2025
 */

#include "mainwindow.h"
#include "pgndialog.h"
#include "NMEA2000_SocketCAN.h"

tNMEA2000_SocketCAN* nmea2000;
extern char can_interface[];

MainWindow* MainWindow::instance = nullptr;


/**
 * @brief Initializes the NMEA2000 communication interface.
 *
 * This function creates a new NMEA2000 interface using the specified CAN interface,
 * sets its mode to listen and node, opens the connection, and assigns a static message handler.
 * It also starts a timer with a 100 ms interval to handle periodic tasks.
 *
 * @note The function assumes that 'can_interface' is properly initialized and accessible.
 */
void MainWindow::initNMEA2000()
{
    nmea2000 = new tNMEA2000_SocketCAN(can_interface);
    if (nmea2000 != nullptr) {
        nmea2000->SetMode(tNMEA2000::N2km_ListenAndNode);
        nmea2000->Open();
        nmea2000->SetMsgHandler(&MainWindow::staticN2kMsgHandler);
        startTimer(100);  // interval in milliseconds — this example triggers every 100 ms
    }
}

/**
 * @brief Handles timer events for the MainWindow.
 *
 * This function is called whenever a timer event occurs. If the NMEA2000 interface
 * is initialized (not nullptr), it parses incoming NMEA2000 messages by invoking
 * the ParseMessages() method.
 *
 * @param event Pointer to the QTimerEvent that triggered this handler.
 */
void MainWindow::timerEvent(QTimerEvent *event) 
{
    if (nmea2000 != nullptr) {
        nmea2000->ParseMessages();
    }
}


/**
 * @brief Constructs a MainWindow object.
 *
 * Initializes the main window UI and sets up the NMEA2000 interface.
 * Captures the current instance of MainWindow for use in static callbacks.
 *
 * @param parent The parent widget, passed to the QMainWindow constructor.
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) 
{
    ui->setupUi(this);
    MainWindow::instance = this;  // Capture 'this' for static callback
    initNMEA2000();
}


/**
 * @brief Destructor for the MainWindow class.
 *
 * Cleans up resources allocated by the MainWindow instance.
 * Deletes the UI object and, if the NMEA2000 interface exists,
 * deletes it to free associated memory.
 */
MainWindow::~MainWindow() 
{
    delete ui;
    if (nmea2000 != nullptr) {
        // Gracefully shutdown the NMEA 2000 stack
        //nmea2000->Close();
        delete nmea2000;
    }
}


void MainWindow::on_sendButton_clicked() 
{
    // Placeholder for sending PGN
    //ui->textEdit->append("Send button clicked");
}

void MainWindow::on_sendPGNButton_clicked()
{
    PGNDialog dialog(this);
    dialog.exec();
}


/**
 * @brief Static handler for N2k messages.
 *
 * This function acts as a static callback for processing N2k messages.
 * If the MainWindow instance exists, it forwards the received message
 * to the instance's handleN2kMsg method for further handling.
 *
 * @param msg The N2k message to be processed.
 */
void MainWindow::staticN2kMsgHandler(const tN2kMsg &msg) {
    if (instance != nullptr) {
        instance->handleN2kMsg(msg);
    }
}

/**
 * @brief Handles an incoming NMEA 2000 message and logs its details.
 *
 * This function extracts relevant information from the provided N2k message,
 * such as PGN, priority, source, and destination, formats it as a string,
 * and appends it to the log text box in the UI for display.
 *
 * @param msg The NMEA 2000 message to process and log.
 */
void MainWindow::handleN2kMsg(const tN2kMsg& msg) {
    QString pgnInfo = QString("PGN: %1, Priority: %2, Source: %3, Destination: %4")
                          .arg(msg.PGN)
                          .arg(msg.Priority)
                          .arg(msg.Source)
                          .arg(msg.Destination);

    // Append to the scrolling text box
    ui->logTextBox->append(pgnInfo);
}


#include "mainwindow.h"
#include "NMEA2000_SocketCAN.h"

tNMEA2000_SocketCAN* nmea2000;
char can_interface[80] = "vcan0";

MainWindow* MainWindow::instance = nullptr;


void MainWindow::initNMEA2000() {
    nmea2000 = new tNMEA2000_SocketCAN(can_interface);
    if (nmea2000 != nullptr) {
        nmea2000->SetMode(tNMEA2000::N2km_ListenAndNode);
        nmea2000->Open();
        nmea2000->SetMsgHandler(&MainWindow::staticN2kMsgHandler);
        startTimer(100);  // interval in milliseconds — this example triggers every 100 ms
    }
}

void MainWindow::timerEvent(QTimerEvent *event) {
    if (nmea2000 != nullptr) {
        nmea2000->ParseMessages();
    }
}


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);\
    MainWindow::instance = this;  // Capture 'this' for static callback
    initNMEA2000();
}


MainWindow::~MainWindow() {
    delete ui;
    if (nmea2000 != nullptr) {
        //nmea2000->Close();
        delete nmea2000;
    }
}

void MainWindow::on_sendButton_clicked() {
    // Placeholder for sending PGN
    //ui->textEdit->append("Send button clicked");
}

void MainWindow::staticN2kMsgHandler(const tN2kMsg &msg) {
    if (instance != nullptr) {
        instance->handleN2kMsg(msg);
    }
}

void MainWindow::handleN2kMsg(const tN2kMsg& msg) {
    QString pgnInfo = QString("PGN: %1, Priority: %2, Source: %3, Destination: %4")
                          .arg(msg.PGN)
                          .arg(msg.Priority)
                          .arg(msg.Source)
                          .arg(msg.Destination);

    // Append to the scrolling text box
    ui->logTextBox->append(pgnInfo);
}


/**
 * @file mainwindow.cpp
 * @brief Implementation of the MainWindow class for tMainWindow::~MainWindow() 
{
    delete ui;
    if (m_deviceList) {
        delete m_deviceList;
        m_deviceList = nullptr;
    }
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
#include "devicelistdialog.h"
#include "NMEA2000_SocketCAN.h"
#include <QDir>
#include <QProcess>
#include <QMessageBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QWidget>
#include <QRegExp>
#include <QToolBar>
#include <QDateTime>

tNMEA2000_SocketCAN* nmea2000;
extern char can_interface[80];

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
    nmea2000->SetMode(tNMEA2000::N2km_ListenAndNode);
    nmea2000->Open();
    nmea2000->SetMsgHandler(staticN2kMsgHandler);
    
    // Create and attach device list
    m_deviceList = new tN2kDeviceList(nmea2000);
    
    startTimer(100);  // Set timer interval to 100 ms
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
    : QMainWindow(parent), ui(new Ui::MainWindow), m_canInterfaceCombo(nullptr), m_deviceList(nullptr)
{
    ui->setupUi(this);
    MainWindow::instance = this;  // Capture 'this' for static callback
    
    // Set up the main window properties for better resizing
    setupMainWindowProperties();
    
    // Set up CAN interface selector
    setupCanInterfaceSelector();
    
    // Initialize with the current interface (from command line or default)
    m_currentInterface = QString(can_interface);
    
    // Populate and set the current interface in the combo box
    populateCanInterfaces();
    
    // Connect buttons
    connect(ui->pushButton_2, &QPushButton::clicked, this, &MainWindow::clearLog);
    connect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::showDeviceList);
    
    initNMEA2000();
}

void MainWindow::setupMainWindowProperties()
{
    // Set minimum and reasonable default sizes
    setMinimumSize(600, 400);
    resize(800, 600);
    
    // Configure the log text box for better display
    ui->logTextBox->setLineWrapMode(QTextEdit::WidgetWidth);
    ui->logTextBox->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->logTextBox->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    // Add some initial content to show the layout
    ui->logTextBox->append("NMEA2000 CAN Interface Tool initialized");
    ui->logTextBox->append("Ready to receive and send CAN messages");
    ui->logTextBox->append("===========================================");
}

void MainWindow::setupCanInterfaceSelector()
{
    // Create a widget to hold the CAN interface selector
    QWidget* canSelectorWidget = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(canSelectorWidget);
    layout->setContentsMargins(5, 5, 5, 5);
    
    // Create label and combo box
    QLabel* label = new QLabel("CAN Interface:");
    m_canInterfaceCombo = new QComboBox();
    m_canInterfaceCombo->setMinimumWidth(120);
    
    // Connect the signal
    connect(m_canInterfaceCombo, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
            this, &MainWindow::on_canInterfaceChanged);
    
    layout->addWidget(label);
    layout->addWidget(m_canInterfaceCombo);
    layout->addStretch(); // Push everything to the left
    
    // Add to the main window toolbar
    ui->toolBar->addWidget(canSelectorWidget);
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

void MainWindow::on_canInterfaceChanged(const QString &interface)
{
    if (interface != m_currentInterface && !interface.isEmpty()) {
        m_currentInterface = interface;
        
        // Update the global can_interface variable
        strncpy(can_interface, interface.toLocal8Bit().data(), sizeof(can_interface) - 1);
        can_interface[sizeof(can_interface) - 1] = '\0';
        
        // Reinitialize NMEA2000 with new interface
        reinitializeNMEA2000();
        
        // Log the interface change
        QString message = QString("Switched to CAN interface: %1").arg(interface);
        ui->logTextBox->append(message);
    }
}

void MainWindow::populateCanInterfaces()
{
    if (!m_canInterfaceCombo) return;
    
    // Get available CAN interfaces
    QStringList interfaces = getAvailableCanInterfaces();
    
    // Clear and populate the combo box
    m_canInterfaceCombo->clear();
    
    if (interfaces.isEmpty()) {
        // If no interfaces found, add default options
        m_canInterfaceCombo->addItem("vcan0");
        m_canInterfaceCombo->addItem("can0");
        m_canInterfaceCombo->addItem("can1");
    } else {
        m_canInterfaceCombo->addItems(interfaces);
    }
    
    // Set the current interface if it exists in the list
    int index = m_canInterfaceCombo->findText(m_currentInterface);
    if (index >= 0) {
        m_canInterfaceCombo->setCurrentIndex(index);
    } else {
        // If current interface not in list, add it and select it
        m_canInterfaceCombo->addItem(m_currentInterface);
        m_canInterfaceCombo->setCurrentText(m_currentInterface);
    }
}

QStringList MainWindow::getAvailableCanInterfaces()
{
    QStringList interfaces;
    
    // Check /sys/class/net/ for network interfaces
    QDir netDir("/sys/class/net");
    if (netDir.exists()) {
        QStringList entries = netDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        
        for (const QString& entry : entries) {
            // Check if it's a CAN interface by looking at the type file
            QFile typeFile(QString("/sys/class/net/%1/type").arg(entry));
            if (typeFile.open(QIODevice::ReadOnly)) {
                QString type = typeFile.readAll().trimmed();
                // CAN interfaces typically have type 280 (ARPHRD_CAN)
                if (type == "280" || entry.startsWith("can") || entry.startsWith("vcan")) {
                    interfaces.append(entry);
                }
                typeFile.close();
            }
        }
    }
    
    // Also use ip command to get CAN interfaces
    QProcess process;
    process.start("ip", QStringList() << "link" << "show" << "type" << "can");
    process.waitForFinished(3000); // 3 second timeout
    
    if (process.exitCode() == 0) {
        QString output = process.readAllStandardOutput();
        QStringList lines = output.split('\n');
        
        for (const QString& line : lines) {
            // Parse lines like "2: vcan0: <NOARP,UP,LOWER_UP> mtu 16 qdisc noqueue state UNKNOWN mode DEFAULT group default qlen 1000"
            if (line.contains(':') && (line.contains("can") || line.contains("vcan"))) {
                QRegExp rx(R"(\d+:\s+(\w+):)");
                if (rx.indexIn(line) != -1) {
                    QString ifname = rx.cap(1);
                    if (!interfaces.contains(ifname)) {
                        interfaces.append(ifname);
                    }
                }
            }
        }
    }
    
    // Sort the interfaces
    interfaces.sort();
    
    return interfaces;
}

void MainWindow::reinitializeNMEA2000()
{
    // Clean up existing NMEA2000 instance
    if (m_deviceList) {
        delete m_deviceList;
        m_deviceList = nullptr;
    }
    if (nmea2000 != nullptr) {
        delete nmea2000;
        nmea2000 = nullptr;
    }
    
    // Initialize with new interface
    initNMEA2000();
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

    // Track PGN instances for conflict detection
    trackPGNInstance(msg);

    // Append to the scrolling text box
    ui->logTextBox->append(pgnInfo);
}

void MainWindow::clearLog()
{
    ui->logTextBox->clear();
    ui->logTextBox->append("NMEA2000 CAN Interface Tool - Log cleared");
    ui->logTextBox->append("Ready to receive and send CAN messages");
    ui->logTextBox->append("===========================================");
}

void MainWindow::showDeviceList()
{
    DeviceListDialog* deviceDialog = new DeviceListDialog(this, m_deviceList);
    deviceDialog->show();
}

// PGN instance tracking methods
void MainWindow::trackPGNInstance(const tN2kMsg& msg) {
    // Only track PGNs that contain instance data
    if (!isPGNWithInstance(msg.PGN)) {
        return;
    }
    
    uint8_t instance = extractInstanceFromPGN(msg);
    if (instance == 255) { // Invalid instance
        return;
    }
    
    QString key = QString("%1_%2").arg(msg.PGN).arg(msg.Source);
    
    PGNInstanceData data;
    data.pgn = msg.PGN;
    data.source = msg.Source;
    data.instance = instance;
    data.lastSeen = QDateTime::currentMSecsSinceEpoch();
    
    m_pgnInstances[key] = data;
    
    // Update conflict detection
    updateInstanceConflicts();
}

uint8_t MainWindow::extractInstanceFromPGN(const tN2kMsg& msg) {
    // Extract instance data based on PGN type
    // Instance is typically the first byte of data for most PGNs
    
    if (msg.DataLen < 1) {
        return 255; // Invalid
    }
    
    uint8_t instance = 255;
    
    switch (msg.PGN) {
        case 127505: // Fluid Level
            if (msg.DataLen >= 1) {
                instance = msg.Data[0]; // Instance is first byte
            }
            break;
            
        case 127508: // Battery Status
            if (msg.DataLen >= 1) {
                instance = msg.Data[0]; // Battery instance is first byte
            }
            break;
            
        case 127502: // Binary Switch Bank Control
            if (msg.DataLen >= 1) {
                instance = msg.Data[0]; // Instance is first byte
            }
            break;
            
        case 130312: // Temperature
            if (msg.DataLen >= 2) {
                instance = msg.Data[1]; // Instance is second byte (after SID)
            }
            break;
            
        case 130314: // Actual Pressure
            if (msg.DataLen >= 2) {
                instance = msg.Data[1]; // Instance is second byte (after SID)
            }
            break;
            
        case 127488: // Engine Parameters, Rapid
            if (msg.DataLen >= 1) {
                instance = msg.Data[0]; // Engine instance is first byte
            }
            break;
            
        default:
            // For other PGNs, assume instance is first byte if present
            if (msg.DataLen >= 1) {
                instance = msg.Data[0];
            }
            break;
    }
    
    return instance;
}

void MainWindow::updateInstanceConflicts() {
    m_instanceConflicts.clear();
    m_conflictingSources.clear();
    
    // Group PGN instances by PGN and instance number
    QMap<QString, QList<PGNInstanceData>> groupedInstances;
    
    for (auto it = m_pgnInstances.begin(); it != m_pgnInstances.end(); ++it) {
        const PGNInstanceData& data = it.value();
        QString groupKey = QString("%1_%2").arg(data.pgn).arg(data.instance);
        groupedInstances[groupKey].append(data);
    }
    
    // Find conflicts (same PGN + instance from different sources)
    for (auto it = groupedInstances.begin(); it != groupedInstances.end(); ++it) {
        const QList<PGNInstanceData>& instances = it.value();
        
        if (instances.size() > 1) {
            // This is a conflict!
            InstanceConflict conflict;
            conflict.pgn = instances[0].pgn;
            conflict.instance = instances[0].instance;
            
            for (const PGNInstanceData& data : instances) {
                conflict.conflictingSources.insert(data.source);
                m_conflictingSources.insert(data.source);
            }
            
            m_instanceConflicts.append(conflict);
        }
    }
}

bool MainWindow::isPGNWithInstance(unsigned long pgn) {
    // List of PGNs that contain instance data
    static QSet<unsigned long> instancePGNs = {
        127488, // Engine Parameters, Rapid
        127502, // Binary Switch Bank Control
        127505, // Fluid Level
        127508, // Battery Status
        130312, // Temperature
        130314, // Actual Pressure
        127245, // Rudder (may have instance in some implementations)
        // Add more PGNs as needed
    };
    
    return instancePGNs.contains(pgn);
}

QList<InstanceConflict> MainWindow::getInstanceConflicts() const {
    return m_instanceConflicts;
}

bool MainWindow::hasInstanceConflicts() const {
    return !m_instanceConflicts.isEmpty();
}

QSet<uint8_t> MainWindow::getConflictingSources() const {
    return m_conflictingSources;
}


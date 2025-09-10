#include "devicemainwindow.h"
#include <cstdint>
#include "pgnlogdialog.h"
#include "pgndialog.h"
#include "pocodevicedialog.h"
#include "zonelightingdialog.h"
#include "LumitecPoco.h"

#ifdef WASM_BUILD
#include "NMEA2000_WASM.h"
#else
#include "NMEA2000_SocketCAN.h"
#endif

#ifdef ENABLE_IPG100_SUPPORT
#include "NMEA2000_IPG100.h"
#endif
#include "instanceconflictanalyzer.h"
#include <NMEA2000.h>
#include <N2kGroupFunction.h>
#include <QDir>
#ifndef WASM_BUILD
#include <QProcess>
#endif
#include <QMessageBox>
#include <QInputDialog>
#include <QHostAddress>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QDebug>
#include <QHBoxLayout>
#include <QWidget>
#include <QDebug>
#include <QRegularExpression>
#include <QToolBar>
#include <QPushButton>
#include <QDateTime>
#include <QApplication>
#include <QMenuBar>
#include <QStatusBar>
#include <QClipboard>
#include <QIcon>
#include <QPixmap>
#include <QSlider>
#include <QDialog>
#include <QVBoxLayout>
#include <QRadioButton>
#include <QButtonGroup>
#include <QDateTime>

tNMEA2000* nmea2000;
extern char can_interface[80];

// Static instance for message handling
DeviceMainWindow* DeviceMainWindow::instance = nullptr;

DeviceMainWindow::DeviceMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_centralWidget(nullptr)
    , m_deviceTable(nullptr)
    , m_analyzeButton(nullptr)
    , m_pgnLogButton(nullptr)
    , m_sendPGNButton(nullptr)
    , m_clearConflictsButton(nullptr)
    , m_statusLabel(nullptr)
    , m_updateTimer(nullptr)
    , m_canInterfaceCombo(nullptr)
    , m_txIndicator(nullptr)
    , m_rxIndicator(nullptr)
    , m_txBlinkTimer(nullptr)
    , m_rxBlinkTimer(nullptr)
    , m_deviceList(nullptr)
    , m_isConnected(false)
    , m_conflictAnalyzer(nullptr)
    , m_hasSeenValidTraffic(false)
    , m_autoDiscoveryTriggered(false)
    , m_messagesReceived(0)
    , m_followUpQueriesScheduled(false)
{
    DeviceMainWindow::instance = this;  // Capture 'this' for static callback
    
    // Initialize the instance conflict analyzer
    m_conflictAnalyzer = new InstanceConflictAnalyzer(this);
    
    setupUI();
    setupMenuBar();
    
    // Initialize with the current interface (from command line or default)
    m_currentInterface = QString(can_interface);
    
    // Populate and set the current interface in the combo box
    populateCanInterfaces();
    
    // Set up auto-refresh timer (update every 2 seconds)
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &DeviceMainWindow::updateDeviceList);
    m_updateTimer->start(2000);
    
    initNMEA2000();
    
    // Initial population
    updateDeviceList();
    
    // Set window icon explicitly
    setWindowIcon(QIcon(":/app_icon.svg"));
    
    setWindowTitle("Lumitec Poco Tester");
    resize(1000, 700);
}

DeviceMainWindow::~DeviceMainWindow()
{
    if (m_updateTimer) {
        m_updateTimer->stop();
    }
    
    if (m_deviceList) {
        delete m_deviceList;
        m_deviceList = nullptr;
    }
    
    if (nmea2000 != nullptr) {
        delete nmea2000;
        nmea2000 = nullptr;
    }
}

void DeviceMainWindow::setupUI()
{
    m_centralWidget = new QWidget();
    setCentralWidget(m_centralWidget);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(m_centralWidget);
    
    // Top toolbar with CAN interface selector and connection controls
    QHBoxLayout* toolbarLayout = new QHBoxLayout();
    
    QLabel* canLabel = new QLabel("CAN Interface:");
    m_canInterfaceCombo = new QComboBox();
    m_canInterfaceCombo->setMinimumWidth(120);
    connect(m_canInterfaceCombo, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
            this, &DeviceMainWindow::onCanInterfaceChanged);
    
#ifdef ENABLE_IPG100_SUPPORT
    // Add IPG100 button
    QPushButton* addIPG100Btn = new QPushButton("Add IPG100...");
    addIPG100Btn->setMaximumWidth(100);
    connect(addIPG100Btn, &QPushButton::clicked, this, &DeviceMainWindow::addManualIPG100);
#endif
    
    // Connection control buttons
    m_connectButton = new QPushButton("Connect");
    m_disconnectButton = new QPushButton("Disconnect");
    connect(m_connectButton, &QPushButton::clicked, this, &DeviceMainWindow::onConnectClicked);
    connect(m_disconnectButton, &QPushButton::clicked, this, &DeviceMainWindow::onDisconnectClicked);
    
    // Auto-discovery checkbox
    m_autoDiscoveryCheckbox = new QCheckBox("Auto-discover devices");
    m_autoDiscoveryCheckbox->setChecked(true); // Default to enabled
    m_autoDiscoveryCheckbox->setToolTip("Automatically request device information when interface comes online");
    
    // Set initial button states
    updateConnectionButtonStates();
    
    // Setup activity indicators
    setupActivityIndicators();
    
    toolbarLayout->addWidget(canLabel);
    toolbarLayout->addWidget(m_canInterfaceCombo);
#ifdef ENABLE_IPG100_SUPPORT
    toolbarLayout->addWidget(addIPG100Btn);
#endif
    toolbarLayout->addSpacing(10); // Add some space between interface and buttons
    toolbarLayout->addWidget(m_connectButton);
    toolbarLayout->addWidget(m_disconnectButton);
    toolbarLayout->addSpacing(10);
    toolbarLayout->addWidget(m_autoDiscoveryCheckbox);
    toolbarLayout->addStretch(); // Push activity indicators to the right
    toolbarLayout->addWidget(new QLabel("RX:"));
    toolbarLayout->addWidget(m_rxIndicator);
    toolbarLayout->addSpacing(5);
    toolbarLayout->addWidget(new QLabel("TX:"));
    toolbarLayout->addWidget(m_txIndicator);
    
    mainLayout->addLayout(toolbarLayout);
    
    // Status label
    m_statusLabel = new QLabel("Scanning for NMEA2000 devices...");
    m_statusLabel->setStyleSheet("font-weight: bold; color: #333; padding: 5px; background-color: #f0f0f0; border: 1px solid #ccc; border-radius: 3px;");
    m_statusLabel->setWordWrap(true);
    mainLayout->addWidget(m_statusLabel);
    
    // Device table
    m_deviceTable = new QTableWidget();
    m_deviceTable->setColumnCount(8);
    
    QStringList headers;
    headers << "Node Address" << "Manufacturer" << "Model ID" << "Serial Number" 
            << "Instance" << "Current Software" << "Installation 1" << "Installation 2";
    m_deviceTable->setHorizontalHeaderLabels(headers);
    
    // Configure table
    m_deviceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_deviceTable->setAlternatingRowColors(true);
    m_deviceTable->horizontalHeader()->setStretchLastSection(true);
    m_deviceTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_deviceTable->verticalHeader()->setVisible(false);
    m_deviceTable->setSortingEnabled(true); // Enable sorting by clicking column headers
    m_deviceTable->sortByColumn(0, Qt::AscendingOrder); // Default sort by Node Address (source address)
    
    // Set smaller font for the table
    QFont tableFont = m_deviceTable->font();
    tableFont.setPointSize(9);
    m_deviceTable->setFont(tableFont);
    
    // Connect row selection signal for conflict highlighting
    connect(m_deviceTable->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &DeviceMainWindow::onRowSelectionChanged);
    
    // Enable context menu for the device table
    m_deviceTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_deviceTable, &QTableWidget::customContextMenuRequested,
            this, &DeviceMainWindow::showDeviceContextMenu);
    
    mainLayout->addWidget(m_deviceTable);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    m_analyzeButton = new QPushButton("Analyze Instance Conflicts");
    m_pgnLogButton = new QPushButton("Show PGN Log");
    m_sendPGNButton = new QPushButton("Send PGN");
    m_clearConflictsButton = new QPushButton("Clear Conflict History");
    
    connect(m_analyzeButton, &QPushButton::clicked, this, &DeviceMainWindow::analyzeInstanceConflicts);
    connect(m_pgnLogButton, &QPushButton::clicked, this, &DeviceMainWindow::showPGNLog);
    connect(m_sendPGNButton, &QPushButton::clicked, this, &DeviceMainWindow::showSendPGNDialog);
    connect(m_clearConflictsButton, &QPushButton::clicked, this, &DeviceMainWindow::clearConflictHistory);
    
    buttonLayout->addWidget(m_analyzeButton);
    buttonLayout->addWidget(m_pgnLogButton);
    buttonLayout->addWidget(m_sendPGNButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_clearConflictsButton);
    
    mainLayout->addLayout(buttonLayout);
}

void DeviceMainWindow::setupMenuBar()
{
    QMenuBar* menuBar = this->menuBar();
    
    // File menu
    QMenu* fileMenu = menuBar->addMenu("&File");
    fileMenu->addAction("&Export Device List...", this, []() {
        QMessageBox::information(nullptr, "Export", "Export functionality coming soon!");
    });
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", this, &QWidget::close);
    
    // Tools menu
    QMenu* toolsMenu = menuBar->addMenu("&Tools");
    toolsMenu->addAction("&Send PGN...", this, &DeviceMainWindow::showSendPGNDialog);
    toolsMenu->addAction("Show PGN &Log", this, &DeviceMainWindow::showPGNLog);
    toolsMenu->addSeparator();
    toolsMenu->addAction("&Request Info from All Devices", this, &DeviceMainWindow::requestInfoFromAllDevices);
    toolsMenu->addSeparator();
    toolsMenu->addAction("&Analyze Instance Conflicts", this, &DeviceMainWindow::analyzeInstanceConflicts);
    toolsMenu->addAction("&Clear Conflict History", this, &DeviceMainWindow::clearConflictHistory);
    
    // Help menu
    QMenu* helpMenu = menuBar->addMenu("&Help");
    helpMenu->addAction("&About", this, [this]() {
        QMessageBox aboutBox(this);
        aboutBox.setWindowTitle("About Lumitec Poco Tester");
        aboutBox.setIconPixmap(QPixmap(":/app_icon.svg").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        aboutBox.setText("<h3>Lumitec Poco Tester</h3>"
                        "<p><b>Version 1.0</b></p>"
                        "<p>Professional NMEA2000 network diagnostic tool with Lumitec Poco protocol support.</p>");
        aboutBox.setInformativeText("<p>Features:</p>"
                                   "<ul>"
                                   "<li>Real-time device discovery and monitoring</li>"
                                   "<li>Instance conflict detection and highlighting</li>"
                                   "<li>Cross-platform CAN interface support</li>"
                                   "<li>Device-specific context menus and actions</li>"
                                   "<li>Live PGN logging and analysis</li>"
                                   "<li>Lumitec Poco protocol support and control</li>"
                                   "</ul>"
                                   "<p>Built with Qt and NMEA2000 library.</p>");
        aboutBox.setStandardButtons(QMessageBox::Ok);
        aboutBox.exec();
    });
}

void DeviceMainWindow::initNMEA2000()
{
    qDebug() << "initNMEA2000() called with interface:" << m_currentInterface;
    
    // Reset automatic discovery tracking
    m_hasSeenValidTraffic = false;
    m_autoDiscoveryTriggered = false;
    m_messagesReceived = 0;
    m_followUpQueriesScheduled = false;
    m_knownDevices.clear();
    m_interfaceStartTime = QDateTime::currentDateTime();
    
    if (nmea2000 != nullptr) {
        delete nmea2000;
        nmea2000 = nullptr;
    }

    // Create and initialize the NMEA2000 interface FIRST
    qDebug() << "Creating NMEA2000 interface for:" << m_currentInterface;
    qDebug() << "Global can_interface variable:" << can_interface;
    
    // Check if this is an IPG100 interface
    if (m_currentInterface.startsWith("IPG100")) {
#ifdef ENABLE_IPG100_SUPPORT
        // Extract IP address from interface name: "IPG100 (192.168.1.100)"
        QRegularExpression regex("IPG100 \\(([0-9.]+)\\)");
        QRegularExpressionMatch match = regex.match(m_currentInterface);
        if (match.hasMatch()) {
            QString ipAddress = match.captured(1);
            qDebug() << "Creating IPG100 interface for IP:" << ipAddress;
            
            auto* ipg100Interface = new tNMEA2000_IPG100(ipAddress.toUtf8().constData());
            nmea2000 = ipg100Interface;
        } else {
            qDebug() << "Invalid IPG100 interface format:" << m_currentInterface;
            return; // Exit early on error
        }
#else
        QMessageBox::information(this, "IPG100 Disabled", "IPG100 support has been disabled in this version.");
        return;
#endif
    } else {
        // Standard SocketCAN interface (or WASM stub)
#ifdef WASM_BUILD
        qDebug() << "Creating WASM stub interface for:" << can_interface;
        nmea2000 = new tNMEA2000_WASM(can_interface);
#else
        qDebug() << "Creating SocketCAN interface for:" << can_interface;
        nmea2000 = new tNMEA2000_SocketCAN(can_interface);
#endif
    }
    
    // Verify the actual interface being used
    verifyCanInterface();
    
    // Set device information
    nmea2000->SetDeviceInformation(1, // Unique number for this device
                                  130, // Device function - PC Gateway
                                  25,  // Device class - Internetwork Device
                                  2046, // Manufacturer code (use a generic one)
                                  4);   // Industry group - Marine
    
    nmea2000->SetMode(tNMEA2000::N2km_ListenAndNode, 22);
    nmea2000->EnableForward(false);
    nmea2000->SetMsgHandler(staticN2kMsgHandler);
    nmea2000->Open();
    
    // NOW create device list after NMEA2000 is open and initialized
    if (m_deviceList) {
        delete m_deviceList;
    }
    m_deviceList = new tN2kDeviceList(nmea2000);
    qDebug() << "Device list created with NMEA2000 interface";
    
    // Update connection state to reflect that we're connected
    m_isConnected = true;
    updateConnectionButtonStates();
    
    // Schedule a broadcast ISO request for product information shortly after coming online
    // This helps trigger responses from all devices on the network
    QTimer::singleShot(2000, this, &DeviceMainWindow::sendInitialBroadcastRequest);
    
    startTimer(100); // Start timer event for regular NMEA2000 processing
}

void DeviceMainWindow::timerEvent(QTimerEvent *event)
{
    Q_UNUSED(event);
    if (nmea2000) {
        nmea2000->ParseMessages();
    }
}

void DeviceMainWindow::staticN2kMsgHandler(const tN2kMsg &msg) {
    if (instance) {
        instance->handleN2kMsg(msg);
    }
}

void DeviceMainWindow::handleN2kMsg(const tN2kMsg& msg) {
    // Let the device list handle the message first to update device information
    if (m_deviceList) {
        m_deviceList->HandleMsg(msg);
    }
    
    // Blink RX indicator for received messages
    blinkRxIndicator();
    
    // Track traffic for automatic device discovery
    m_messagesReceived++;
    if (!m_hasSeenValidTraffic) {
        m_hasSeenValidTraffic = true;
        qDebug() << "First valid NMEA2000 traffic detected, will trigger automatic discovery in" << AUTO_DISCOVERY_DELAY_MS << "ms";
    }
    
    // Check if we should trigger automatic device discovery
    if (!m_autoDiscoveryTriggered && m_hasSeenValidTraffic && 
        m_autoDiscoveryCheckbox && m_autoDiscoveryCheckbox->isChecked() &&
        m_messagesReceived >= MIN_MESSAGES_FOR_DISCOVERY &&
        m_interfaceStartTime.msecsTo(QDateTime::currentDateTime()) >= AUTO_DISCOVERY_DELAY_MS) {
        
        m_autoDiscoveryTriggered = true;
        qDebug() << "Triggering automatic device discovery after" << m_messagesReceived << "messages";
        
        // Trigger discovery with a short delay to let current message processing complete
        QTimer::singleShot(500, this, &DeviceMainWindow::triggerAutomaticDeviceDiscovery);
        
        // Schedule follow-up queries to check for devices with missing information
        scheduleFollowUpQueries();
    }
    
    // Update device activity tracking
    updateDeviceActivity(msg.Source);
    
    // Track PGN instances for conflict detection
    m_conflictAnalyzer->trackPGNMessage(msg);

    // Handle Lumitec Poco specific messages
    if (msg.PGN == LUMITEC_PGN_61184) {
        handleLumitecPocoMessage(msg);
    }
    
    // Handle Product Information responses (PGN 126996)
    if (msg.PGN == N2kPGNProductInformation) {
        handleProductInformationResponse(msg);
    }
    
    // Handle Configuration Information responses (PGN 126998)
    if (msg.PGN == N2kPGNConfigurationInformation) {
        handleConfigurationInformationResponse(msg);
    }
    
    // Handle Group Function messages (PGN 126208) - including acknowledgments
    if (msg.PGN == 126208UL) {
        handleGroupFunctionMessage(msg);
    }

    // Forward to all PGN log dialogs
    for (PGNLogDialog* dialog : m_pgnLogDialogs) {
        if (dialog && dialog->isVisible()) {
            dialog->appendMessage(msg);
        }
    }
}

void DeviceMainWindow::populateCanInterfaces()
{
    qDebug() << "populateCanInterfaces() called";
    
    if (!m_canInterfaceCombo) return;
    
    m_canInterfaceCombo->clear();
    
    // Get available CAN interfaces
    QStringList interfaces = getAvailableCanInterfaces();
    
    qDebug() << "Available CAN interfaces:" << interfaces;
    qDebug() << "Current interface (m_currentInterface):" << m_currentInterface;
    
    if (interfaces.isEmpty()) {
        m_canInterfaceCombo->addItem("No CAN interfaces found");
        m_canInterfaceCombo->setEnabled(false);
    } else {
        m_canInterfaceCombo->addItems(interfaces);
        m_canInterfaceCombo->setEnabled(true);
        
        // Set current interface if it exists in the list
        int currentIndex = interfaces.indexOf(m_currentInterface);
        qDebug() << "Index of current interface in list:" << currentIndex;
        if (currentIndex >= 0) {
            m_canInterfaceCombo->setCurrentIndex(currentIndex);
        }
    }
}

QStringList DeviceMainWindow::getAvailableCanInterfaces()
{
    QStringList interfaces;
    
    // First, scan for physical CAN interfaces
    QDir sysClassNet("/sys/class/net");
    if (sysClassNet.exists()) {
        QStringList entries = sysClassNet.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        
        foreach (const QString& entry, entries) {
            QString typePath = QString("/sys/class/net/%1/type").arg(entry);
            QFile typeFile(typePath);
            if (typeFile.open(QIODevice::ReadOnly)) {
                QString type = typeFile.readAll().trimmed();
                if (type == "280") { // CAN interface type
                    interfaces.append(entry);
                }
                typeFile.close();
            }
        }
    }
    
#ifdef ENABLE_IPG100_SUPPORT
    // Discover IPG100 devices on the network
    qDebug() << "Scanning for IPG100 devices...";
    std::vector<std::string> ipg100DevicesVec = tNMEA2000_IPG100::discoverIPG100Devices(3000);
    QStringList ipg100Devices;
    for (const auto& device : ipg100DevicesVec) {
        ipg100Devices << QString("IPG100 (%1)").arg(QString::fromStdString(device));
    }
    interfaces.append(ipg100Devices);
#endif
    
    // If no interfaces found, add some common ones for testing
    if (interfaces.isEmpty()) {
        interfaces << "can0" << "can1" << "vcan0" << "vcan1";
    }
    
    return interfaces;
}

void DeviceMainWindow::onCanInterfaceChanged(const QString &interface)
{
    if (interface != m_currentInterface && !interface.isEmpty()) {
        qDebug() << "=== CAN INTERFACE SWITCHING ===";
        qDebug() << "Switching FROM:" << m_currentInterface;
        qDebug() << "Switching TO:" << interface;
        
        m_currentInterface = interface;
        strncpy(can_interface, interface.toLocal8Bit().data(), sizeof(can_interface) - 1);
        can_interface[sizeof(can_interface) - 1] = '\0';
        
        qDebug() << "Global can_interface variable updated to:" << can_interface;
        
        reinitializeNMEA2000();
        
        qDebug() << "=== INTERFACE SWITCH COMPLETED ===";
    }
}

void DeviceMainWindow::reinitializeNMEA2000()
{
    qDebug() << "reinitializeNMEA2000() called";
    
    if (nmea2000 != nullptr) {
        qDebug() << "Deleting existing NMEA2000 instance";
        delete nmea2000;
        nmea2000 = nullptr;
    }
    
    // Clear the device table and activity tracking when switching interfaces
    qDebug() << "Clearing device list for interface switch";
    m_deviceTable->setRowCount(0);
    m_deviceActivity.clear();
    
    // Clear conflict history when changing interface
    clearConflictHistory();
    
    qDebug() << "Reinitializing NMEA2000 with interface:" << m_currentInterface;
    initNMEA2000();
    updateDeviceList();
}

void DeviceMainWindow::verifyCanInterface()
{
    if (!nmea2000) {
        qDebug() << "ERROR: Cannot verify interface - nmea2000 is null";
        return;
    }
    
    qDebug() << "=== INTERFACE VERIFICATION ===";
    qDebug() << "Target interface:" << m_currentInterface;
    
    // Check if this is an IPG100 interface
    if (m_currentInterface.startsWith("IPG100")) {
#ifdef ENABLE_IPG100_SUPPORT
        auto* ipg100Interface = dynamic_cast<tNMEA2000_IPG100*>(nmea2000);
        if (ipg100Interface) {
            qDebug() << "IPG100 interface detected";
            qDebug() << "IP Address:" << QString::fromStdString(ipg100Interface->getDiscoveredIP());
            qDebug() << "Connection status:" << (ipg100Interface->isConnected() ? "Connected" : "Disconnected");
        } else {
            qDebug() << "ERROR: Interface claims to be IPG100 but cast failed";
        }
#else
        qDebug() << "IPG100 interface verification disabled";
#endif
        qDebug() << "=== END VERIFICATION ===";
        return;
    }
    
    // Original SocketCAN verification logic
    QString interfaceCheck;
    
    // Method 1: Check if interface exists in system
    QString sysPath = QString("/sys/class/net/%1").arg(m_currentInterface);
    QFileInfo sysInfo(sysPath);
    bool interfaceExists = sysInfo.exists() && sysInfo.isDir();
    
    qDebug() << "System path exists:" << sysPath << "=" << interfaceExists;
    
    // Method 2: Check interface statistics (different for physical vs virtual)
    QString statsPath = QString("/sys/class/net/%1/statistics/rx_packets").arg(m_currentInterface);
    QFile statsFile(statsPath);
    if (statsFile.open(QIODevice::ReadOnly)) {
        QString rxPackets = statsFile.readAll().trimmed();
        qDebug() << "Interface RX packets:" << rxPackets;
        statsFile.close();
    }
    
    // Method 3: Check interface type (280 = CAN)
    QString typePath = QString("/sys/class/net/%1/type").arg(m_currentInterface);
    QFile typeFile(typePath);
    if (typeFile.open(QIODevice::ReadOnly)) {
        QString type = typeFile.readAll().trimmed();
        qDebug() << "Interface type:" << type << (type == "280" ? "(CAN)" : "(NOT CAN!)");
        typeFile.close();
    }
    
    // Method 4: Show which interfaces are currently UP
#ifndef WASM_BUILD
    QProcess process;
    process.start("ip", QStringList() << "link" << "show" << "up");
    process.waitForFinished();
    QString output = process.readAllStandardOutput();
    bool isUp = output.contains(m_currentInterface);
    qDebug() << "Interface is UP:" << isUp;
#else
    // WASM: Network interface checking not available in browser
    bool isUp = false;
    qDebug() << "Interface is UP: N/A (WASM build)";
#endif
    
    qDebug() << "=== END VERIFICATION ===";
}

#ifdef ENABLE_IPG100_SUPPORT
void DeviceMainWindow::addManualIPG100()
{
    bool ok;
    QString ipAddress = QInputDialog::getText(this, "Add IPG100 Device", 
                                            "Enter IPG100 IP Address:", 
                                            QLineEdit::Normal, 
                                            "192.168.1.100", &ok);
    
    if (!ok || ipAddress.isEmpty()) {
        return;
    }
    
    // Validate IP address format
    QHostAddress addr(ipAddress);
    if (addr.isNull()) {
        QMessageBox::warning(this, "Invalid IP Address", 
                           "Please enter a valid IP address (e.g., 192.168.1.100)");
        return;
    }
    
    // Test connectivity
    qDebug() << "Testing IPG100 connectivity to" << ipAddress;
    if (!tNMEA2000_IPG100::isIPG100Available(ipAddress.toUtf8().constData(), 3000)) {
        int ret = QMessageBox::question(this, "IPG100 Not Responding", 
                                       QString("Cannot connect to IPG100 at %1.\n\n"
                                              "This could be because:\n"
                                              "• The IP address is incorrect\n"
                                              "• The IPG100 is offline\n"
                                              "• Network connectivity issues\n"
                                              "• Firewall blocking the connection\n\n"
                                              "Add it anyway?").arg(ipAddress),
                                       QMessageBox::Yes | QMessageBox::No);
        if (ret != QMessageBox::Yes) {
            return;
        }
    }
    
    // Add to interface list
    QString interfaceName = QString("IPG100 (%1)").arg(ipAddress);
    
    // Check if already exists
    for (int i = 0; i < m_canInterfaceCombo->count(); i++) {
        if (m_canInterfaceCombo->itemText(i) == interfaceName) {
            QMessageBox::information(this, "IPG100 Already Added", 
                                   "This IPG100 device is already in the interface list.");
            m_canInterfaceCombo->setCurrentIndex(i);
            return;
        }
    }
    
    // Add and select the new interface
    m_canInterfaceCombo->addItem(interfaceName);
    m_canInterfaceCombo->setCurrentText(interfaceName);
    
    QMessageBox::information(this, "IPG100 Added", 
                           QString("IPG100 at %1 has been added to the interface list.\n\n"
                                  "The connection will be established when you select this interface.").arg(ipAddress));
}
#endif

// Include all the device table population and conflict detection methods from devicelistdialog.cpp
void DeviceMainWindow::updateDeviceList()
{
    if (!nmea2000) {
        m_statusLabel->setText("NMEA2000 interface not initialized");
        m_statusLabel->setStyleSheet("font-weight: bold; color: red; padding: 5px; background-color: #ffe6e6; border: 1px solid #ff9999; border-radius: 3px;");
        return;
    }
    
    // Check for device timeouts
    checkDeviceTimeouts();
    
    populateDeviceTable();
}

void DeviceMainWindow::populateDeviceTable()
{
    if (!m_deviceList) {
        m_statusLabel->setText("Device list not available");
        m_statusLabel->setStyleSheet("font-weight: bold; color: red; padding: 5px; background-color: #ffe6e6; border: 1px solid #ff9999; border-radius: 3px;");
        return;
    }
    
    // Save current sort state
    int sortColumn = m_deviceTable->horizontalHeader()->sortIndicatorSection();
    Qt::SortOrder sortOrder = m_deviceTable->horizontalHeader()->sortIndicatorOrder();
    
    // Save current selection (if any)
    QString selectedDeviceAddress;
    int currentRow = m_deviceTable->currentRow();
    if (currentRow >= 0 && currentRow < m_deviceTable->rowCount()) {
        QTableWidgetItem* nodeItem = m_deviceTable->item(currentRow, 0);
        if (nodeItem) {
            selectedDeviceAddress = nodeItem->text();
        }
    }
    
    // Disable sorting during update
    m_deviceTable->setSortingEnabled(false);
    // Clear the table to prevent duplicates
    m_deviceTable->setRowCount(0);
    // Create a map to track existing devices and their table positions
    QMap<uint8_t, int> existingDeviceRows;
    
    // If table is not empty, preserve existing devices
    for (int row = 0; row < m_deviceTable->rowCount(); row++) {
        QTableWidgetItem* nodeItem = m_deviceTable->item(row, 0);
        if (nodeItem) {
            bool ok;
            uint8_t source = nodeItem->text().toUInt(&ok, 16);
            if (ok) {
                existingDeviceRows[source] = row;
            }
        }
    }
    
    int deviceCount = 0;
    
    // First pass: update existing devices and mark active ones
    for (uint8_t source = 0; source < N2kMaxBusDevices; source++) {
        const tNMEA2000::tDevice* device = m_deviceList->FindDeviceBySource(source);
        if (device) {
            bool isActive = m_deviceActivity.contains(source) && m_deviceActivity[source].isActive;
            
            if (existingDeviceRows.contains(source)) {
                // Update existing device
                int row = existingDeviceRows[source];
                updateDeviceTableRow(row, source, device, isActive);
                m_deviceActivity[source].tableRow = row;
                deviceCount++;
                
                // Add to known devices if not already there
                m_knownDevices.insert(source);
            } else {
                // New device - add to end
                int newRow = m_deviceTable->rowCount();
                m_deviceTable->insertRow(newRow);
                updateDeviceTableRow(newRow, source, device, isActive);
                m_deviceActivity[source].tableRow = newRow;
                deviceCount++;
                
                // Check if this is truly a new device (not just a reconnection)
                if (!m_knownDevices.contains(source)) {
                    m_knownDevices.insert(source);
                    qDebug() << "New device detected:" << QString("0x%1").arg(source, 2, 16, QChar('0')).toUpper() 
                             << "- scheduling information query";
                    
                    // Schedule query for this new device with a short delay to let it settle
                    QTimer::singleShot(1000, [this, source]() {
                        queryNewDevice(source);
                    });
                }
            }
        }
    }
    
    // Gray out inactive devices
    grayOutInactiveDevices();
    
    // Update status
    if (deviceCount == 0) {
        m_statusLabel->setText("No NMEA2000 devices detected on the network");
        m_statusLabel->setStyleSheet("font-weight: bold; color: orange; padding: 5px; background-color: #fff3cd; border: 1px solid #ffeaa7; border-radius: 3px;");
    } else {
        QString statusText = QString("Found %1 NMEA2000 device(s) - Auto-updating every 2 seconds").arg(deviceCount);
        QString statusStyle = "font-weight: bold; color: green; padding: 5px; background-color: #e6ffe6; border: 1px solid #99ff99; border-radius: 3px;";
        
        // Update conflict analysis before checking
        m_conflictAnalyzer->updateConflictAnalysis();
        
        // Check for instance conflicts and update status accordingly
        if (m_conflictAnalyzer->hasConflicts()) {
            statusText += QString(" - WARNING: %1 instance conflict(s) detected!").arg(m_conflictAnalyzer->getConflictCount());
            statusStyle = "font-weight: bold; color: red; padding: 5px; background-color: #ffe6e6; border: 1px solid #ff9999; border-radius: 3px;";
        }
        
        m_statusLabel->setText(statusText);
        m_statusLabel->setStyleSheet(statusStyle);
    }
    
    // Analyze and highlight instance conflicts
    m_conflictAnalyzer->highlightConflictsInTable(m_deviceTable);
    
    // Resize columns to content
    m_deviceTable->resizeColumnsToContents();
    // Restore previous sort
    m_deviceTable->setSortingEnabled(true);
    m_deviceTable->sortByColumn(sortColumn, sortOrder);
    
    // Restore previous selection (if any)
    if (!selectedDeviceAddress.isEmpty()) {
        for (int row = 0; row < m_deviceTable->rowCount(); row++) {
            QTableWidgetItem* nodeItem = m_deviceTable->item(row, 0);
            if (nodeItem && nodeItem->text() == selectedDeviceAddress) {
                m_deviceTable->selectRow(row);
                break;
            }
        }
    }
    
    // Update PGN dialog device list if it exists
    updatePGNDialogDeviceList();
}

// Slot implementations
void DeviceMainWindow::onRowSelectionChanged()
{
    // Implementation similar to DeviceListDialog::onRowSelectionChanged()
    // Will add later - for now just a stub
}

void DeviceMainWindow::showDeviceContextMenu(const QPoint& position)
{
    QTableWidgetItem* item = m_deviceTable->itemAt(position);
    if (!item) {
        return; // No item clicked
    }
    
    int row = item->row();
    if (row < 0 || row >= m_deviceTable->rowCount()) {
        return;
    }
    
    // Get device information from the table
    QTableWidgetItem* nodeAddressItem = m_deviceTable->item(row, 0); // Node Address column
    QTableWidgetItem* manufacturerItem = m_deviceTable->item(row, 1); // Manufacturer column
    QTableWidgetItem* modelItem = m_deviceTable->item(row, 2);        // Model ID column
    
    if (!nodeAddressItem) {
        return;
    }
    
    QString nodeAddress = nodeAddressItem->text();
    QString manufacturer = manufacturerItem ? manufacturerItem->text() : "Unknown";
    QString model = modelItem ? modelItem->text() : "Unknown";
    
    // Convert hex node address to decimal for PGN sending
    bool ok;
    uint8_t sourceAddress = nodeAddressItem->text().toUInt(&ok, 16);
    if (!ok) {
        return;
    }
    
    // Create context menu using QMenuBar's functionality
    QMenuBar* contextMenuBar = new QMenuBar(this);
    QMenu* contextMenu = contextMenuBar->addMenu(QString("Device 0x%1").arg(nodeAddress));
    
    // Add device info header (non-clickable)
    QAction* headerAction = contextMenu->addAction(QString("Device 0x%1 (%2)").arg(nodeAddress, manufacturer));
    headerAction->setEnabled(false);
    QFont boldFont = headerAction->font();
    boldFont.setBold(true);
    headerAction->setFont(boldFont);
    
    contextMenu->addSeparator();
    
    // Send PGN to Device action
    QAction* sendPGNAction = contextMenu->addAction("Send PGN to Device...");
    connect(sendPGNAction, &QAction::triggered, [this, sourceAddress, nodeAddress]() {
        showSendPGNToDevice(sourceAddress, nodeAddress);
    });
    
    // Get Device Details action
    QAction* deviceDetailsAction = contextMenu->addAction("Show Device Details...");
    connect(deviceDetailsAction, &QAction::triggered, [this, row]() {
        showDeviceDetails(row);
    });
    
    contextMenu->addSeparator();
    
    // Device Information Requests submenu
    QMenu* requestMenu = contextMenu->addMenu("Request Device Information");
    
    // Request Product Information action
    QAction* productInfoAction = requestMenu->addAction("Product Information (PGN 126996)");
    connect(productInfoAction, &QAction::triggered, [this, sourceAddress]() {
        requestProductInformation(sourceAddress);
    });
    
    // Query Device Configuration action
    QAction* queryConfigAction = requestMenu->addAction("Configuration Information (PGN 126998)");
    connect(queryConfigAction, &QAction::triggered, [this, sourceAddress]() {
        queryDeviceConfiguration(sourceAddress);
    });
    
    // Request Supported PGNs action
    QAction* supportedPGNsAction = requestMenu->addAction("Supported PGNs (PGN 126464)");
    connect(supportedPGNsAction, &QAction::triggered, [this, sourceAddress]() {
        requestSupportedPGNs(sourceAddress);
    });
    
    requestMenu->addSeparator();
    
    // Request All Information action
    QAction* requestAllAction = requestMenu->addAction("Request All Information...");
    connect(requestAllAction, &QAction::triggered, [this, sourceAddress]() {
        requestAllInformation(sourceAddress);
    });
    requestAllAction->setIcon(QIcon(":/icons/refresh.png"));  // Add icon if available
    
    // Add Lumitec-specific menu items if this is a Lumitec device
    if (manufacturer == "Lumitec") {
        contextMenu->addSeparator();
        
        // Lumitec Control submenu
        QMenu* lumitecMenu = contextMenu->addMenu("Lumitec Poco Control");
        
        // New Poco Device Dialog
        QAction* pocoDialogAction = lumitecMenu->addAction("Poco Device Control...");
        connect(pocoDialogAction, &QAction::triggered, [this, sourceAddress, nodeAddress]() {
            showPocoDeviceDialog(sourceAddress, nodeAddress);
        });
        
        // Zone Lighting Control
        QAction* zoneLightingAction = lumitecMenu->addAction("Zone Lighting Control...");
        connect(zoneLightingAction, &QAction::triggered, [this, sourceAddress]() {
            onPocoZoneLightingControlRequested(sourceAddress);
        });
        
        lumitecMenu->addSeparator();
        
        QAction* switchAction = lumitecMenu->addAction("Switch Action...");
        connect(switchAction, &QAction::triggered, [this, sourceAddress, nodeAddress]() {
            showLumitecSwitchActionDialog(sourceAddress, nodeAddress);
        });
        lumitecMenu->addSeparator();
        QAction* customControlAction = lumitecMenu->addAction("Custom Color Control...");
        connect(customControlAction, &QAction::triggered, [this, sourceAddress, nodeAddress]() {
            showLumitecColorDialog(sourceAddress, nodeAddress);
        });
    }

    contextMenu->addSeparator();

    // Instance Conflict Details (only show if device has conflicts)
    if (m_conflictAnalyzer && m_conflictAnalyzer->hasConflictForSource(sourceAddress)) {
        QAction* conflictDetailsAction = contextMenu->addAction("Show Instance Conflicts...");
        conflictDetailsAction->setIcon(QIcon(":/icons/warning.png")); // Warning icon if available
        connect(conflictDetailsAction, &QAction::triggered, [this, sourceAddress, nodeAddress]() {
            showInstanceConflictDetails(sourceAddress, nodeAddress);
        });
        contextMenu->addSeparator();
    }

    // Edit Installation Labels action
    QAction* editLabelsAction = contextMenu->addAction("Edit Installation Labels...");
    connect(editLabelsAction, &QAction::triggered, [this, sourceAddress, nodeAddress]() {
        editInstallationLabels(sourceAddress, nodeAddress);
    });

    contextMenu->addSeparator();

    // Copy Node Address action
    QAction* copyAddressAction = contextMenu->addAction("Copy Node Address");
    connect(copyAddressAction, &QAction::triggered, [nodeAddress]() {
        QApplication::clipboard()->setText(nodeAddress);
    });

    // Show PGN Log for Device action
    QAction* pgnLogAction = contextMenu->addAction("Show PGN Log for Device");
    connect(pgnLogAction, &QAction::triggered, [this, sourceAddress]() {
        showPGNLogForDevice(sourceAddress);
    });

    // Show context menu at the clicked position
    contextMenu->exec(m_deviceTable->mapToGlobal(position));

    // Clean up
    delete contextMenuBar;
}

void DeviceMainWindow::showInstanceConflictDetails(uint8_t sourceAddress, const QString& nodeAddress)
{
    if (!m_conflictAnalyzer) {
        QMessageBox::information(this, "Instance Conflicts", "Instance conflict analyzer is not available.");
        return;
    }
    
    QList<InstanceConflict> conflicts = m_conflictAnalyzer->getConflictDetailsForSource(sourceAddress);
    
    if (conflicts.isEmpty()) {
        QMessageBox::information(this, "Instance Conflicts", 
                                QString("No instance conflicts found for device 0x%1").arg(nodeAddress));
        return;
    }
    
    // Create dialog to show conflict details
    QDialog dialog(this);
    dialog.setWindowTitle(QString("Instance Conflicts - Device 0x%1").arg(nodeAddress));
    dialog.setModal(true);
    dialog.resize(600, 400);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    // Header info
    QLabel* headerLabel = new QLabel(QString("Instance conflicts detected for device 0x%1:").arg(nodeAddress));
    QFont headerFont = headerLabel->font();
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);
    layout->addWidget(headerLabel);
    
    // Explanation
    QLabel* explanationLabel = new QLabel(
        "The following PGN+Instance combinations are being transmitted by multiple devices,\n"
        "which violates NMEA2000 protocol and causes ambiguity for listeners:"
    );
    explanationLabel->setWordWrap(true);
    explanationLabel->setStyleSheet("color: #666; margin: 10px 0px;");
    layout->addWidget(explanationLabel);
    
    // Table showing conflicts
    QTableWidget* conflictTable = new QTableWidget();
    conflictTable->setColumnCount(3);
    conflictTable->setHorizontalHeaderLabels({"PGN", "Instance", "Description"});
    
    // Calculate total rows needed (one row per conflict, plus device rows for each conflict)
    int totalRows = 0;
    for (const InstanceConflict& conflict : conflicts) {
        totalRows += 1 + conflict.conflictingSources.size(); // Header row + device rows
    }
    
    conflictTable->setRowCount(totalRows);
    conflictTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    conflictTable->setAlternatingRowColors(true);
    
    // Populate table with conflicts and devices
    int currentRow = 0;
    for (const InstanceConflict& conflict : conflicts) {
        // Conflict header row
        QTableWidgetItem* pgnItem = new QTableWidgetItem(QString::number(conflict.pgn));
        pgnItem->setTextAlignment(Qt::AlignCenter);
        pgnItem->setFont(QFont(pgnItem->font().family(), pgnItem->font().pointSize(), QFont::Bold));
        conflictTable->setItem(currentRow, 0, pgnItem);
        
        QTableWidgetItem* instanceItem = new QTableWidgetItem(QString::number(conflict.instance));
        instanceItem->setTextAlignment(Qt::AlignCenter);
        instanceItem->setFont(QFont(instanceItem->font().family(), instanceItem->font().pointSize(), QFont::Bold));
        conflictTable->setItem(currentRow, 1, instanceItem);
        
        QString pgnName = InstanceConflictAnalyzer::getPGNName(conflict.pgn);
        QTableWidgetItem* descItem = new QTableWidgetItem(pgnName);
        descItem->setFont(QFont(descItem->font().family(), descItem->font().pointSize(), QFont::Bold));
        conflictTable->setItem(currentRow, 2, descItem);
        
        // Set background color for header row
        for (int col = 0; col < 3; ++col) {
            QTableWidgetItem* item = conflictTable->item(currentRow, col);
            if (item) {
                item->setBackground(QBrush(QColor(240, 240, 240))); // Light gray header
            }
        }
        
        currentRow++;
        
        // Device rows
        for (uint8_t addr : conflict.conflictingSources) {
            // Empty PGN and Instance columns for device rows
            conflictTable->setItem(currentRow, 0, new QTableWidgetItem(""));
            conflictTable->setItem(currentRow, 1, new QTableWidgetItem(""));
            
            // Device name in description column
            QString deviceName = getDeviceDisplayName(addr);
            QTableWidgetItem* deviceItem = new QTableWidgetItem(QString("    • %1").arg(deviceName));
            conflictTable->setItem(currentRow, 2, deviceItem);
            
            // Store device information in the item data for context menu
            deviceItem->setData(Qt::UserRole, addr);  // Device address
            deviceItem->setData(Qt::UserRole + 1, static_cast<qulonglong>(conflict.pgn));  // PGN
            deviceItem->setData(Qt::UserRole + 2, conflict.instance);  // Current instance
            
            // Highlight current device's row
            if (addr == sourceAddress) {
                for (int col = 0; col < 3; ++col) {
                    QTableWidgetItem* item = conflictTable->item(currentRow, col);
                    if (item) {
                        item->setBackground(QBrush(QColor(255, 200, 200))); // Light red
                    }
                }
            }
            
            currentRow++;
        }
    }
    
    // Enable context menu for the conflict table
    conflictTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(conflictTable, &QTableWidget::customContextMenuRequested,
            [this, conflictTable, &dialog](const QPoint& position) {
                showConflictTableContextMenu(conflictTable, position, &dialog);
            });
    
    // Auto-resize columns
    conflictTable->resizeColumnsToContents();
    conflictTable->horizontalHeader()->setStretchLastSection(true);
    
    layout->addWidget(conflictTable);
    
    // Resolution advice
    QLabel* adviceLabel = new QLabel(
        "<b>Resolution:</b> Each device should use a unique instance number for each PGN type. "
        "Consult your device documentation to configure different instance numbers."
    );
    adviceLabel->setWordWrap(true);
    adviceLabel->setStyleSheet("background-color: #fff3cd; border: 1px solid #ffeaa7; padding: 10px; border-radius: 5px; margin: 10px 0px;");
    layout->addWidget(adviceLabel);
    
    // Close button
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    QPushButton* closeButton = new QPushButton("Close");
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    buttonLayout->addWidget(closeButton);
    layout->addLayout(buttonLayout);
    
    dialog.exec();
}

void DeviceMainWindow::editInstallationLabels(uint8_t sourceAddress, const QString& nodeAddress)
{
    Q_UNUSED(sourceAddress); // Currently using nodeAddress to find device in table
    
    // Get current installation labels from the device table
    QString currentLabel1, currentLabel2;
    
    // Find the device row in the table
    for (int row = 0; row < m_deviceTable->rowCount(); ++row) {
        QTableWidgetItem* nodeAddressItem = m_deviceTable->item(row, 0);
        if (nodeAddressItem && nodeAddressItem->text() == nodeAddress) {
            QTableWidgetItem* label1Item = m_deviceTable->item(row, 6); // Installation 1 column
            QTableWidgetItem* label2Item = m_deviceTable->item(row, 7); // Installation 2 column
            
            currentLabel1 = label1Item ? label1Item->text() : "";
            currentLabel2 = label2Item ? label2Item->text() : "";
            break;
        }
    }
    
    // Create dialog for editing installation labels
    QDialog dialog(this);
    dialog.setWindowTitle(QString("Edit Installation Labels - Device 0x%1").arg(nodeAddress));
    dialog.setModal(true);
    dialog.resize(450, 280);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    // Header info
    QLabel* headerLabel = new QLabel(QString("Edit installation labels for device 0x%1").arg(nodeAddress));
    QFont headerFont = headerLabel->font();
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);
    layout->addWidget(headerLabel);
    
    // Explanation
    QLabel* explanationLabel = new QLabel(
        "Installation Description fields identify the physical location or purpose of this device."
    );
    explanationLabel->setWordWrap(true);
    explanationLabel->setStyleSheet("color: #666; margin: 10px 0px;");
    layout->addWidget(explanationLabel);
    
    // Encoding selection
    QLabel* encodingLabel = new QLabel("Text Encoding:");
    encodingLabel->setStyleSheet("font-weight: bold; margin-top: 10px;");
    layout->addWidget(encodingLabel);
    
    QRadioButton* asciiRadio = new QRadioButton("ASCII (up to 70 characters each field)");
    QRadioButton* unicodeRadio = new QRadioButton("Unicode UTF-16 (up to 35 characters each field)");
    
    // Default to ASCII for simplicity unless we detect non-ASCII characters
    bool hasNonAscii = false;
    for (const QString& text : {currentLabel1, currentLabel2}) {
        for (const QChar& ch : text) {
            if (ch.unicode() > 127) {
                hasNonAscii = true;
                break;
            }
        }
        if (hasNonAscii) break;
    }
    
    if (hasNonAscii) {
        unicodeRadio->setChecked(true);
    } else {
        asciiRadio->setChecked(true);
    }
    
    QButtonGroup* encodingGroup = new QButtonGroup();
    encodingGroup->addButton(asciiRadio, 0);  // 0 = ASCII
    encodingGroup->addButton(unicodeRadio, 1); // 1 = Unicode
    
    layout->addWidget(asciiRadio);
    layout->addWidget(unicodeRadio);
    
    // Installation Description 1
    QLabel* label1Label = new QLabel("Installation Description 1:");
    layout->addWidget(label1Label);
    
    QLineEdit* label1Edit = new QLineEdit(currentLabel1);
    label1Edit->setMaxLength(70); // Will be adjusted based on encoding selection
    label1Edit->setPlaceholderText("e.g., \"Main Engine Port Side\" or \"Forward Bilge\"");
    layout->addWidget(label1Edit);
    
    // Character count for label 1
    QLabel* count1Label = new QLabel();
    count1Label->setAlignment(Qt::AlignRight);
    count1Label->setStyleSheet("color: #888; font-size: 10px;");
    layout->addWidget(count1Label);
    
    // Installation Description 2
    QLabel* label2Label = new QLabel("Installation Description 2:");
    layout->addWidget(label2Label);
    
    QLineEdit* label2Edit = new QLineEdit(currentLabel2);
    label2Edit->setMaxLength(70); // Will be adjusted based on encoding selection
    label2Edit->setPlaceholderText("e.g., \"Sensor #1\" or \"Additional Notes\"");
    layout->addWidget(label2Edit);
    
    // Character count for label 2
    QLabel* count2Label = new QLabel();
    count2Label->setAlignment(Qt::AlignRight);
    count2Label->setStyleSheet("color: #888; font-size: 10px;");
    layout->addWidget(count2Label);
    
    // Function to update character counts and validation based on encoding
    auto updateCountsAndLimits = [count1Label, count2Label, label1Edit, label2Edit, encodingGroup]() {
        bool isUnicode = (encodingGroup->checkedId() == 1);
        QString text1 = label1Edit->text();
        QString text2 = label2Edit->text();
        
        int limit, count1, count2;
        QString unit;
        
        if (isUnicode) {
            // Unicode mode: limit is 35 characters (70 bytes in UTF-16)
            limit = 35;
            count1 = text1.length();  // Each QChar is one UTF-16 character
            count2 = text2.length();
            unit = "characters";
            label1Edit->setMaxLength(35);
            label2Edit->setMaxLength(35);
        } else {
            // ASCII mode: limit is 70 characters (70 bytes in ASCII)
            limit = 70;
            count1 = text1.toUtf8().length();  // Byte count for ASCII
            count2 = text2.toUtf8().length();
            unit = "characters";
            label1Edit->setMaxLength(70);
            label2Edit->setMaxLength(70);
            
            // In ASCII mode, warn about non-ASCII characters
            bool hasNonAscii1 = false, hasNonAscii2 = false;
            for (const QChar& ch : text1) {
                if (ch.unicode() > 127) { hasNonAscii1 = true; break; }
            }
            for (const QChar& ch : text2) {
                if (ch.unicode() > 127) { hasNonAscii2 = true; break; }
            }
            
            if (hasNonAscii1 || hasNonAscii2) {
                // Show warning about non-ASCII characters in ASCII mode
                count1Label->setText(QString("%1/%2 %3 (⚠ Non-ASCII detected)").arg(count1).arg(limit).arg(unit));
                count2Label->setText(QString("%1/%2 %3 (⚠ Non-ASCII detected)").arg(count2).arg(limit).arg(unit));
                count1Label->setStyleSheet("color: #ff6600; font-size: 10px;");
                count2Label->setStyleSheet("color: #ff6600; font-size: 10px;");
                return;
            }
        }
        
        count1Label->setText(QString("%1/%2 %3").arg(count1).arg(limit).arg(unit));
        count2Label->setText(QString("%1/%2 %3").arg(count2).arg(limit).arg(unit));
        
        // Color coding for limits
        count1Label->setStyleSheet(QString("color: %1; font-size: 10px;").arg(count1 > limit ? "#d32f2f" : "#888"));
        count2Label->setStyleSheet(QString("color: %1; font-size: 10px;").arg(count2 > limit ? "#d32f2f" : "#888"));
    };
    
    // Connect signals for real-time updates
    connect(label1Edit, &QLineEdit::textChanged, updateCountsAndLimits);
    connect(label2Edit, &QLineEdit::textChanged, updateCountsAndLimits);
    connect(encodingGroup, QOverload<int>::of(&QButtonGroup::idClicked), updateCountsAndLimits);
    
    // Initial count update
    updateCountsAndLimits();
    
    // Validation function
    auto validateInput = [label1Edit, label2Edit, encodingGroup]() -> bool {
        bool isUnicode = (encodingGroup->checkedId() == 1);
        
        for (QLineEdit* edit : {label1Edit, label2Edit}) {
            QString text = edit->text();
            
            if (isUnicode) {
                // Unicode mode: 35 character limit
                if (text.length() > 35) {
                    return false;
                }
            } else {
                // ASCII mode: 70 byte limit, and check for non-ASCII characters
                if (text.toUtf8().length() > 70) {
                    return false;
                }
                for (const QChar& ch : text) {
                    if (ch.unicode() > 127) {
                        return false; // Non-ASCII character in ASCII mode
                    }
                }
            }
        }
        return true;
    };
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    QPushButton* cancelButton = new QPushButton("Cancel");
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    buttonLayout->addWidget(cancelButton);
    
    QPushButton* okButton = new QPushButton("OK");
    okButton->setDefault(true);
    connect(okButton, &QPushButton::clicked, [&dialog, validateInput, encodingGroup]() {
        if (validateInput()) {
            dialog.accept();
        } else {
            bool isUnicode = (encodingGroup->checkedId() == 1);
            QString errorMsg;
            if (isUnicode) {
                errorMsg = "One or more fields exceed the 35 character limit.\n"
                          "Unicode mode: NMEA2000 uses UTF-16 encoding with a 70-byte limit (35 characters max).";
            } else {
                errorMsg = "One or more fields exceed the 70 character limit or contain non-ASCII characters.\n"
                          "ASCII mode: Only standard ASCII characters (0-127) are allowed, up to 70 characters.";
            }
            QMessageBox::warning(&dialog, "Invalid Input", errorMsg);
        }
    });
    buttonLayout->addWidget(okButton);
    
    layout->addLayout(buttonLayout);
    
    // Show dialog and handle result
    if (dialog.exec() == QDialog::Accepted) {
        QString newLabel1 = label1Edit->text();
        QString newLabel2 = label2Edit->text();
        bool useUnicode = (encodingGroup->checkedId() == 1);
        
        // Update the device table
        for (int row = 0; row < m_deviceTable->rowCount(); ++row) {
            QTableWidgetItem* nodeAddressItem = m_deviceTable->item(row, 0);
            if (nodeAddressItem && nodeAddressItem->text() == nodeAddress) {
                // Update Installation 1
                QTableWidgetItem* label1Item = m_deviceTable->item(row, 6);
                if (!label1Item) {
                    label1Item = new QTableWidgetItem();
                    m_deviceTable->setItem(row, 6, label1Item);
                }
                label1Item->setText(newLabel1);
                
                // Update Installation 2
                QTableWidgetItem* label2Item = m_deviceTable->item(row, 7);
                if (!label2Item) {
                    label2Item = new QTableWidgetItem();
                    m_deviceTable->setItem(row, 7, label2Item);
                }
                label2Item->setText(newLabel2);
                
                break;
            }
        }
        
        // Send Group Function command to update device configuration
        bool updateSent = sendConfigurationUpdate(sourceAddress, newLabel1, newLabel2, useUnicode);
        
        // Show confirmation
        QString confirmMessage;
        if (updateSent) {
            confirmMessage = QString("Installation labels for device 0x%1 have been updated in the display "
                                   "and a configuration update command has been sent to the device.\n\n"
                                   "The device may take a moment to process the update and respond with "
                                   "the new configuration information.")
                           .arg(nodeAddress);
        } else {
            confirmMessage = QString("Installation labels for device 0x%1 have been updated in the display.\n\n"
                                   "Warning: Failed to send configuration update command to the device. "
                                   "The changes are only visible in this application.")
                           .arg(nodeAddress);
        }
        QMessageBox::information(this, "Labels Updated", confirmMessage);
    }
}

QString DeviceMainWindow::getDeviceDisplayName(uint8_t sourceAddress) const
{
    if (!m_deviceList) {
        return QString("0x%1").arg(sourceAddress, 2, 16, QChar('0')).toUpper();
    }
    
    const tNMEA2000::tDevice* device = m_deviceList->FindDeviceBySource(sourceAddress);
    if (!device) {
        return QString("0x%1 (Unknown)").arg(sourceAddress, 2, 16, QChar('0')).toUpper();
    }
    
    // Get manufacturer name
    uint16_t manufacturerCode = device->GetManufacturerCode();
    QString manufacturerName = getManufacturerName(manufacturerCode);
    
    // Get model ID
    QString modelId = "Unknown";
    const char* modelIdStr = device->GetModelID();
    if (modelIdStr && strlen(modelIdStr) > 0) {
        modelId = QString(modelIdStr);
    }
    
    // Get installation description 1 (device name/location)
    QString installDesc = "";
    const char* installDesc1Ptr = device->GetInstallationDescription1();
    if (installDesc1Ptr && strlen(installDesc1Ptr) > 0) {
        installDesc = QString(installDesc1Ptr);
    }
    
    // Format the display name
    QString displayName = QString("0x%1").arg(sourceAddress, 2, 16, QChar('0')).toUpper();
    
    if (manufacturerName != "Unknown" && modelId != "Unknown") {
        displayName += QString(" (%1 %2)").arg(manufacturerName, modelId);
    } else if (manufacturerName != "Unknown") {
        displayName += QString(" (%1)").arg(manufacturerName);
    }
    
    if (!installDesc.isEmpty()) {
        displayName += QString(" - %1").arg(installDesc);
    }
    
    return displayName;
}

void DeviceMainWindow::showConflictTableContextMenu(QTableWidget* table, const QPoint& position, QDialog* parentDialog)
{
    QTableWidgetItem* item = table->itemAt(position);
    if (!item) {
        return; // No item clicked
    }
    
    // Check if this is a device row (has device address data)
    QVariant deviceAddressVar = item->data(Qt::UserRole);
    if (!deviceAddressVar.isValid()) {
        return; // This is a header row or empty row
    }
    
    uint8_t deviceAddress = deviceAddressVar.toUInt();
    unsigned long pgn = item->data(Qt::UserRole + 1).toULongLong();
    uint8_t currentInstance = item->data(Qt::UserRole + 2).toUInt();
    
    // Create context menu
    QMenu contextMenu(table);
    
    // Add device info header (non-clickable)
    QString deviceName = getDeviceDisplayName(deviceAddress);
    QAction* headerAction = contextMenu.addAction(QString("Device: %1").arg(deviceName));
    headerAction->setEnabled(false);
    QFont boldFont = headerAction->font();
    boldFont.setBold(true);
    headerAction->setFont(boldFont);
    
    contextMenu.addSeparator();
    
    // Change Instance action
    QAction* changeInstanceAction = contextMenu.addAction(QString("Change Instance (currently %1)...").arg(currentInstance));
    connect(changeInstanceAction, &QAction::triggered, [this, deviceAddress, pgn, currentInstance, parentDialog]() {
        changeDeviceInstance(deviceAddress, pgn, currentInstance, parentDialog);
    });
    
    contextMenu.addSeparator();
    
    // Show Device Details action
    QAction* deviceDetailsAction = contextMenu.addAction("Show Device Details...");
    connect(deviceDetailsAction, &QAction::triggered, [this, deviceAddress]() {
        // Find the device in the main table to get the row
        for (int row = 0; row < m_deviceTable->rowCount(); ++row) {
            QTableWidgetItem* nodeItem = m_deviceTable->item(row, 0);
            if (nodeItem) {
                bool ok;
                uint8_t tableAddress = nodeItem->text().toUInt(&ok, 16);
                if (ok && tableAddress == deviceAddress) {
                    showDeviceDetails(row);
                    break;
                }
            }
        }
    });
    
    // Show context menu at the clicked position
    contextMenu.exec(table->mapToGlobal(position));
}

void DeviceMainWindow::changeDeviceInstance(uint8_t deviceAddress, unsigned long pgn, uint8_t currentInstance, QDialog* parentDialog)
{
    // Create dialog for instance change
    QDialog dialog(parentDialog);
    dialog.setWindowTitle(QString("Change Instance - Device 0x%1").arg(deviceAddress, 2, 16, QChar('0')).toUpper());
    dialog.setModal(true);
    dialog.setFixedSize(400, 250);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    // Device information
    QString deviceName = getDeviceDisplayName(deviceAddress);
    QLabel* deviceLabel = new QLabel(QString("<b>Device:</b> %1").arg(deviceName));
    deviceLabel->setWordWrap(true);
    layout->addWidget(deviceLabel);
    
    // PGN information
    QString pgnName = InstanceConflictAnalyzer::getPGNName(pgn);
    QLabel* pgnLabel = new QLabel(QString("<b>PGN:</b> %1 (%2)").arg(pgn).arg(pgnName));
    layout->addWidget(pgnLabel);
    
    layout->addSpacing(10);
    
    // Current instance
    QLabel* currentLabel = new QLabel(QString("<b>Current Instance:</b> %1").arg(currentInstance));
    layout->addWidget(currentLabel);
    
    // New instance input
    QLabel* newLabel = new QLabel("<b>New Instance:</b>");
    layout->addWidget(newLabel);
    
    // Get suggested instance
    uint8_t suggestedInstance = suggestAvailableInstance(pgn, deviceAddress);
    
    QSpinBox* instanceSpinBox = new QSpinBox();
    instanceSpinBox->setRange(0, 253); // Valid NMEA2000 instance range
    instanceSpinBox->setValue(suggestedInstance); // Start with suggestion instead of current
    instanceSpinBox->setToolTip("Enter a new instance number (0-253) that doesn't conflict with other devices");
    
    // Add a horizontal layout for the spinbox and suggestion button
    QHBoxLayout* instanceLayout = new QHBoxLayout();
    instanceLayout->addWidget(instanceSpinBox);
    
    QPushButton* useSuggestionButton = new QPushButton(QString("Use %1").arg(suggestedInstance));
    useSuggestionButton->setToolTip("Use the suggested conflict-free instance number");
    useSuggestionButton->setMaximumWidth(80);
    connect(useSuggestionButton, &QPushButton::clicked, [instanceSpinBox, suggestedInstance]() {
        instanceSpinBox->setValue(suggestedInstance);
    });
    instanceLayout->addWidget(useSuggestionButton);
    
    layout->addLayout(instanceLayout);
    
    // Suggestion info label
    QLabel* suggestionLabel = new QLabel(
        QString("<font color='green'><b>💡 Suggestion:</b></font> Instance %1 is available and conflict-free for this PGN.")
        .arg(suggestedInstance)
    );
    suggestionLabel->setWordWrap(true);
    suggestionLabel->setStyleSheet("background-color: #d4edda; border: 1px solid #c3e6cb; padding: 8px; border-radius: 5px; margin: 5px 0px;");
    layout->addWidget(suggestionLabel);
    
    // Warning label
    QLabel* warningLabel = new QLabel(
        "<font color='red'><b>Warning:</b></font> This will send a configuration command to the device. "
        "Make sure the new instance number is unique and the device supports remote configuration."
    );
    warningLabel->setWordWrap(true);
    warningLabel->setStyleSheet("background-color: #fff3cd; border: 1px solid #ffeaa7; padding: 8px; border-radius: 5px; margin: 10px 0px;");
    layout->addWidget(warningLabel);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    QPushButton* cancelButton = new QPushButton("Cancel");
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    buttonLayout->addWidget(cancelButton);
    
    QPushButton* changeButton = new QPushButton("Change Instance");
    changeButton->setDefault(true);
    connect(changeButton, &QPushButton::clicked, [this, &dialog, instanceSpinBox, currentInstance, pgn, deviceAddress, suggestedInstance]() {
        uint8_t newInstance = instanceSpinBox->value();
        
        if (newInstance == currentInstance) {
            QMessageBox::information(&dialog, "No Change", "The new instance is the same as the current instance. No change needed.");
            return;
        }
        
        // Check if the selected instance would create a conflict
        QSet<uint8_t> usedInstances = m_conflictAnalyzer->getUsedInstancesForPGN(pgn, deviceAddress);
        if (usedInstances.contains(newInstance)) {
            QMessageBox::StandardButton reply = QMessageBox::question(&dialog, 
                "Potential Conflict", 
                QString("Instance %1 is already used by another device for this PGN.\n"
                       "This will create a conflict.\n\n"
                       "Suggested conflict-free instance: %2\n\n"
                       "Do you want to proceed anyway?")
                .arg(newInstance)
                .arg(suggestedInstance),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
                
            if (reply == QMessageBox::No) {
                return;
            }
        }
        
        dialog.accept();
    });
    buttonLayout->addWidget(changeButton);
    
    layout->addLayout(buttonLayout);
    
    if (dialog.exec() == QDialog::Accepted) {
        uint8_t newInstance = instanceSpinBox->value();
        
        // Send the actual NMEA2000 instance change command
        if (sendInstanceChangeCommand(deviceAddress, pgn, newInstance)) {
            QMessageBox::information(parentDialog, "Instance Change", 
                                    QString("Instance change command sent to device 0x%1:\n"
                                           "PGN: %2 (%3)\n"
                                           "Old Instance: %4\n"
                                           "New Instance: %5\n\n"
                                           "The command has been sent. The device should acknowledge "
                                           "the change if it supports remote configuration.\n\n"
                                           "Conflict analysis will automatically refresh in a few seconds "
                                           "to show the updated status.")
                                    .arg(deviceAddress, 2, 16, QChar('0')).toUpper()
                                    .arg(pgn)
                                    .arg(InstanceConflictAnalyzer::getPGNName(pgn))
                                    .arg(currentInstance)
                                    .arg(newInstance));
        } else {
            QMessageBox::warning(parentDialog, "Command Failed", 
                                QString("Failed to send instance change command to device 0x%1.\n"
                                       "Please check that NMEA2000 is connected and the device "
                                       "supports remote configuration.")
                                .arg(deviceAddress, 2, 16, QChar('0')).toUpper());
        }
    }
}

bool DeviceMainWindow::sendInstanceChangeCommand(uint8_t deviceAddress, unsigned long pgn, uint8_t newInstance)
{
    if (!nmea2000) {
        qDebug() << "NMEA2000 not initialized, cannot send instance change command";
        return false;
    }

    // Get the field number for the instance in this PGN
    uint8_t instanceFieldNumber = getInstanceFieldNumber(pgn);
    if (instanceFieldNumber == 0) {
        qDebug() << "Unknown instance field number for PGN" << pgn;
        return false;
    }

    tN2kMsg msg;
    msg.SetPGN(126208UL);  // Group Function PGN
    msg.Priority = 3;      // Standard priority for commands
    msg.Destination = deviceAddress;
    
    // Group Function Header
    msg.AddByte(1);  // N2kgfc_Command - Command Group Function
    msg.Add3ByteInt(pgn);  // Target PGN for instance change
    msg.AddByte(0x08);  // Priority Setting (standard value for commands)
    msg.AddByte(1);  // Number of parameter pairs (we're changing 1 field)
    
    // Parameter pair: Field number and new value
    msg.AddByte(instanceFieldNumber);  // Field number for instance
    msg.AddByte(newInstance);  // New instance value
    
    bool success = nmea2000->SendMsg(msg);
    if (success) {
        qDebug() << "Sent instance change command to device" 
                 << QString("0x%1").arg(deviceAddress, 2, 16, QChar('0'))
                 << "for PGN" << pgn << "- field" << instanceFieldNumber 
                 << "set to instance" << newInstance;
        
        // Schedule automatic conflict re-analysis after a delay
        // This gives the device time to process the command and start sending data with new instance
        QTimer::singleShot(3000, [this, deviceAddress, pgn, newInstance]() {
            if (m_conflictAnalyzer) {
                qDebug() << "Performing automatic conflict re-analysis after instance change";
                m_conflictAnalyzer->updateConflictAnalysis();
                
                // Update the device table to refresh conflict highlighting
                populateDeviceTable();
                
                // Show updated status
                QString statusMessage;
                if (m_conflictAnalyzer->hasConflicts()) {
                    statusMessage = QString("Instance change complete. %1 conflict(s) still detected.")
                                   .arg(m_conflictAnalyzer->getConflictCount());
                } else {
                    statusMessage = "Instance change complete. No conflicts detected!";
                }
                
                if (m_statusLabel) {
                    m_statusLabel->setText(statusMessage);
                    m_statusLabel->setStyleSheet(m_conflictAnalyzer->hasConflicts() ? 
                                               "color: orange;" : "color: green;");
                    
                    // Reset status color after 5 seconds
                    QTimer::singleShot(5000, [this]() {
                        if (m_statusLabel) {
                            m_statusLabel->setStyleSheet("");
                        }
                    });
                }
                
                qDebug() << statusMessage;
            }
        });
        
        // Blink TX indicator for transmitted messages
        if (m_txIndicator) {
            m_txIndicator->setStyleSheet("background-color: red; border-radius: 10px;");
            QTimer::singleShot(200, [this]() {
                if (m_txIndicator) {
                    m_txIndicator->setStyleSheet("background-color: darkred; border-radius: 10px;");
                }
            });
        }
    } else {
        qDebug() << "Failed to send instance change command to device" 
                 << QString("0x%1").arg(deviceAddress, 2, 16, QChar('0'));
    }
    
    return success;
}

bool DeviceMainWindow::sendConfigurationUpdate(uint8_t targetAddress, const QString& installDesc1, const QString& installDesc2, bool useUnicode)
{
    if (!nmea2000) {
        qDebug() << "Cannot send configuration update: NMEA2000 interface not available";
        return false;
    }
    
    // Send Group Function command to update PGN 126998 fields
    tN2kMsg msg;
    msg.SetPGN(126208UL);  // Group Function PGN
    msg.Priority = 3;      // Standard priority for commands
    msg.Destination = targetAddress;
    
    // Group Function Header
    msg.AddByte(1);  // N2kgfc_Command - Command Group Function
    msg.Add3ByteInt(N2kPGNConfigurationInformation);  // Target PGN 126998
    msg.AddByte(0x08);  // Priority Setting (standard value for commands)
    msg.AddByte(2);  // Number of parameter pairs (we're changing 2 fields)
    
    // Parameter pair 1: Installation Description 1 (Field 1)
    msg.AddByte(1);  // Field number for Installation Description 1
    
    if (useUnicode) {
        // Unicode mode: Convert QString to UTF-8 with SOH prefix for Unicode support
        QByteArray desc1Utf8 = QByteArray("\x01") + installDesc1.toUtf8();
        msg.AddVarStr(desc1Utf8.constData(), false, 70, 35);
    } else {
        // ASCII mode: Convert QString to Latin-1 (ASCII compatible) without prefix
        QByteArray desc1Ascii = installDesc1.toLatin1();
        msg.AddVarStr(desc1Ascii.constData(), false, 70, 70);
    }
    
    // Parameter pair 2: Installation Description 2 (Field 2)
    msg.AddByte(2);  // Field number for Installation Description 2
    
    if (useUnicode) {
        // Unicode mode: Convert QString to UTF-8 with SOH prefix for Unicode support
        QByteArray desc2Utf8 = QByteArray("\x01") + installDesc2.toUtf8();
        msg.AddVarStr(desc2Utf8.constData(), false, 70, 35);
    } else {
        // ASCII mode: Convert QString to Latin-1 (ASCII compatible) without prefix
        QByteArray desc2Ascii = installDesc2.toLatin1();
        msg.AddVarStr(desc2Ascii.constData(), false, 70, 70);
    }
    
    bool success = nmea2000->SendMsg(msg);
    if (success) {
        qDebug() << "Sent configuration update command to device" 
                 << QString("0x%1").arg(targetAddress, 2, 16, QChar('0'))
                 << "using" << (useUnicode ? "Unicode UTF-16" : "ASCII") << "encoding"
                 << "- Install Desc 1:" << installDesc1 
                 << "- Install Desc 2:" << installDesc2;
        
        // Blink TX indicator for transmitted messages
        blinkTxIndicator();
        
        // Log the sent message to all PGN log dialogs
        for (PGNLogDialog* dialog : m_pgnLogDialogs) {
            if (dialog && dialog->isVisible()) {
                dialog->appendSentMessage(msg);
            }
        }
        
        // Schedule a configuration information request after a delay to get updated values
        QTimer::singleShot(2000, [this, targetAddress]() {
            queryDeviceConfiguration(targetAddress);
            qDebug() << "Requesting updated configuration from device" 
                     << QString("0x%1").arg(targetAddress, 2, 16, QChar('0'));
        });
        
    } else {
        qDebug() << "Failed to send configuration update command to device" 
                 << QString("0x%1").arg(targetAddress, 2, 16, QChar('0'));
    }
    
    return success;
}

uint8_t DeviceMainWindow::getInstanceFieldNumber(unsigned long pgn) const
{
    // Return the field number for the instance in various PGNs
    // Based on NMEA2000 standard and common practice
    switch (pgn) {
        // Most PGNs have instance as field 1 (first field after SID if present)
        case 127488: // Engine Parameters, Rapid
        case 127489: // Engine Parameters, Dynamic
        case 127505: // Fluid Level
        case 127508: // Battery Status
        case 127509: // Inverter Status
        case 127513: // Battery Configuration Status
        case 130561: // Poco Zone Control
            return 1;  // Instance is typically field 1
            
        case 127502: // Binary Switch Bank Control
            return 2;  // Instance is field 2 (after switch bank indicator)
            
        case 130312: // Temperature
        case 130314: // Actual Pressure
        case 130316: // Temperature, Extended Range
            return 2;  // Instance is field 2 (after SID)
            
        default:
            // For unknown PGNs, assume instance is field 1
            // This is the most common case
            return 1;
    }
}

// Dialog for selecting switch ID and action for Lumitec Poco
void DeviceMainWindow::showLumitecSwitchActionDialog(uint8_t targetAddress, const QString& nodeAddress) {
    QDialog dialog(this);
    dialog.setWindowTitle(QString("Lumitec Switch Action - Device 0x%1").arg(nodeAddress));
    dialog.setModal(true);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    // Switch ID selector
    QHBoxLayout* switchLayout = new QHBoxLayout();
    QLabel* switchLabel = new QLabel("Switch ID:");
    QSpinBox* switchSpin = new QSpinBox();
    switchSpin->setRange(1, 16); // Adjust max as needed
    switchSpin->setValue(1);
    switchLayout->addWidget(switchLabel);
    switchLayout->addWidget(switchSpin);
    layout->addLayout(switchLayout);

    // Action selector
    QHBoxLayout* actionLayout = new QHBoxLayout();
    QLabel* actionLabel = new QLabel("Action:");
    QComboBox* actionCombo = new QComboBox();
    actionCombo->addItem("Turn Light On", QVariant(ACTION_ON));
    actionCombo->addItem("Turn Light Off", QVariant(ACTION_OFF));
    actionCombo->addItem("Set White", QVariant(ACTION_WHITE));
    actionCombo->addItem("Set Red", QVariant(ACTION_RED));
    actionCombo->addItem("Set Green", QVariant(ACTION_GREEN));
    actionCombo->addItem("Set Blue", QVariant(ACTION_BLUE));
    actionLayout->addWidget(actionLabel);
    actionLayout->addWidget(actionCombo);
    layout->addLayout(actionLayout);

    // Dialog buttons
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        int switchId = switchSpin->value();
        int actionId = actionCombo->currentData().toInt();
        sendLumitecSimpleAction(targetAddress, actionId, switchId);
    }
}

void DeviceMainWindow::showPGNLog()
{
    qDebug() << "showPGNLog: Creating new PGNLogDialog instance...";
    
    // Always create a new dialog for multi-instance support
    PGNLogDialog* newDialog = new PGNLogDialog(this);
    
    // Set up device name resolver
    newDialog->setDeviceNameResolver([this](uint8_t address) {
        return getDeviceName(address);
    });
    
    // Connect the destroyed signal to our cleanup slot
    connect(newDialog, &QObject::destroyed, this, &DeviceMainWindow::onPGNLogDialogDestroyed);
    
    // Add to our list of dialogs
    m_pgnLogDialogs.append(newDialog);
    
    qDebug() << "showPGNLog: About to clear all filters...";
    // Clear all filters for general PGN log view
    newDialog->clearAllFilters();
    qDebug() << "showPGNLog: Filters cleared";
    
    qDebug() << "showPGNLog: About to show dialog...";
    newDialog->show();
    qDebug() << "showPGNLog: Dialog shown";
    
    qDebug() << "showPGNLog: About to raise dialog...";
    newDialog->raise();
    qDebug() << "showPGNLog: Dialog raised";
    
    qDebug() << "showPGNLog: About to activate window...";
    newDialog->activateWindow();
    qDebug() << "showPGNLog: Window activated - completed successfully";
    
    qDebug() << "showPGNLog: Total PGN dialogs now: " << m_pgnLogDialogs.size();
}

void DeviceMainWindow::showSendPGNDialog()
{
    PGNDialog* pgnDialog = new PGNDialog(this);
    pgnDialog->exec();
    delete pgnDialog;
}

void DeviceMainWindow::showSendPGNToDevice(uint8_t targetAddress, const QString& nodeAddress)
{
    PGNDialog* pgnDialog = new PGNDialog(this);
    
    // Set the destination field to the target device's node address
    pgnDialog->setDestinationAddress(targetAddress);
    pgnDialog->setWindowTitle(QString("Send PGN to Device 0x%1").arg(nodeAddress));
    
    pgnDialog->exec();
    delete pgnDialog;
}

void DeviceMainWindow::showDeviceDetails(int row)
{
    if (row < 0 || row >= m_deviceTable->rowCount()) {
        return;
    }
    
    // Gather all device information from the table
    QString nodeAddress = m_deviceTable->item(row, 0) ? m_deviceTable->item(row, 0)->text() : "Unknown";
    QString manufacturer = m_deviceTable->item(row, 1) ? m_deviceTable->item(row, 1)->text() : "Unknown";
    QString modelId = m_deviceTable->item(row, 2) ? m_deviceTable->item(row, 2)->text() : "Unknown";
    QString serialNumber = m_deviceTable->item(row, 3) ? m_deviceTable->item(row, 3)->text() : "Unknown";
    QString instance = m_deviceTable->item(row, 4) ? m_deviceTable->item(row, 4)->text() : "Unknown";
    QString software = m_deviceTable->item(row, 5) ? m_deviceTable->item(row, 5)->text() : "Unknown";
    QString installDesc1 = m_deviceTable->item(row, 6) ? m_deviceTable->item(row, 6)->text() : "Unknown";
    QString installDesc2 = m_deviceTable->item(row, 7) ? m_deviceTable->item(row, 7)->text() : "Unknown";
    
    // Get additional device details from the device list
    bool ok;
    uint8_t source = nodeAddress.toUInt(&ok, 16);
    QString additionalInfo = "";
    
    if (ok && m_deviceList) {
        const tNMEA2000::tDevice* device = m_deviceList->FindDeviceBySource(source);
        if (device) {
            additionalInfo += QString("Device Function: %1\n").arg(device->GetDeviceFunction());
            additionalInfo += QString("Device Class: %1\n").arg(device->GetDeviceClass());
            additionalInfo += QString("Manufacturer Code: %1\n").arg(device->GetManufacturerCode());
            additionalInfo += QString("Industry Group: %1\n").arg(device->GetIndustryGroup());
            
            // Check if device has instance conflicts
            if (m_conflictAnalyzer->hasConflictForSource(source)) {
                additionalInfo += "\n⚠️  WARNING: This device has instance conflicts!\n";
                
                // Get the conflicting PGNs for this source
                QString conflictInfo = m_conflictAnalyzer->getConflictInfoForSource(source);
                if (!conflictInfo.isEmpty()) {
                    additionalInfo += conflictInfo;
                }
            }
        }
    }
    
    QString detailsText = QString(
        "Node Address: 0x%1\n"
        "Manufacturer: %2\n"
        "Model ID: %3\n"
        "Serial Number: %4\n"
        "Device Instance: %5\n"
        "Software Version: %6\n"
        "Installation 1: %7\n"
        "Installation 2: %8\n\n"
        "Additional Info:\n%9"
    ).arg(nodeAddress, manufacturer, modelId, serialNumber, instance, software, installDesc1, installDesc2, additionalInfo);
    
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(QString("Device Details - 0x%1").arg(nodeAddress));
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setText(detailsText);  // Show all details directly in the main text area
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
}

void DeviceMainWindow::queryDeviceConfiguration(uint8_t targetAddress)
{
    if (!nmea2000) {
        QMessageBox::warning(this, "Error", "NMEA2000 interface not available");
        return;
    }
    
    // Send ISO Request for Configuration Information (PGN 126998)
    tN2kMsg N2kMsg;
    SetN2kPGN59904(N2kMsg, targetAddress, N2kPGNConfigurationInformation);
    
    if (nmea2000->SendMsg(N2kMsg)) {
        // Track this request so we can show details when we get the response
        m_pendingConfigInfoRequests.insert(targetAddress);
        
        // Blink TX indicator for transmitted messages
        blinkTxIndicator();
        
        // Log the sent message to all PGN log dialogs
        for (PGNLogDialog* dialog : m_pgnLogDialogs) {
            if (dialog && dialog->isVisible()) {
                dialog->appendSentMessage(N2kMsg);
            }
        }
        
        qDebug() << "Configuration information request sent to device" << 
                   QString("0x%1").arg(targetAddress, 2, 16, QChar('0')).toUpper();
    } else {
        qDebug() << "Failed to send configuration information request to device" << 
                   QString("0x%1").arg(targetAddress, 2, 16, QChar('0')).toUpper();
    }
}

void DeviceMainWindow::requestProductInformation(uint8_t targetAddress)
{
    // Send PGN 59904 (ISO Request) requesting PGN 126996 (Product Information)
    if (!nmea2000) {
        QMessageBox::warning(this, "Error", "NMEA2000 interface not available");
        return;
    }
    
    tN2kMsg N2kMsg;
    SetN2kPGN59904(N2kMsg, targetAddress, N2kPGNProductInformation);
    
    if (nmea2000->SendMsg(N2kMsg)) {
        // Blink TX indicator for transmitted messages
        blinkTxIndicator();
        
        // Track that we've requested product info from this device
        m_pendingProductInfoRequests.insert(targetAddress);
        
        // Cancel any existing retry timer for this device
        cancelProductInfoRetry(targetAddress);
        
        // Start a retry timer for this device
        QTimer* retryTimer = new QTimer();
        retryTimer->setSingleShot(true);
        connect(retryTimer, &QTimer::timeout, [this, targetAddress]() {
            retryProductInformation(targetAddress);
        });
        m_productInfoRetryTimers[targetAddress] = retryTimer;
        retryTimer->start(PRODUCT_INFO_RETRY_TIMEOUT_MS);
        
        // Log the sent message to all PGN log dialogs
        for (PGNLogDialog* dialog : m_pgnLogDialogs) {
            if (dialog && dialog->isVisible()) {
                dialog->appendSentMessage(N2kMsg);
            }
        }
        
        qDebug() << "Product information request sent to device" << 
                   QString("0x%1").arg(targetAddress, 2, 16, QChar('0')).toUpper();
    } else {
        qDebug() << "Failed to send product information request to device" << 
                   QString("0x%1").arg(targetAddress, 2, 16, QChar('0')).toUpper();
    }
}

void DeviceMainWindow::requestSupportedPGNs(uint8_t targetAddress)
{
    if (!nmea2000) {
        QMessageBox::warning(this, "Error", "NMEA2000 interface not available");
        return;
    }
    
    // Send ISO Request for Supported PGNs (PGN 126464)
    tN2kMsg N2kMsg;
    SetN2kPGN59904(N2kMsg, targetAddress, 126464L);
    
    if (nmea2000->SendMsg(N2kMsg)) {
        blinkTxIndicator();
        
        // Log the sent message to all PGN log dialogs
        for (PGNLogDialog* dialog : m_pgnLogDialogs) {
            if (dialog && dialog->isVisible()) {
                dialog->appendSentMessage(N2kMsg);
            }
        }
        
        qDebug() << "Supported PGNs request sent to device" << 
                   QString("0x%1").arg(targetAddress, 2, 16, QChar('0')).toUpper();
    } else {
        qDebug() << "Failed to send supported PGNs request to device" << 
                   QString("0x%1").arg(targetAddress, 2, 16, QChar('0')).toUpper();
    }
}

void DeviceMainWindow::requestAllInformation(uint8_t targetAddress)
{
    if (!nmea2000) {
        qDebug() << "Cannot request information: NMEA2000 interface not available";
        return;
    }
    
    qDebug() << "Sending comprehensive information requests to device"
             << QString("0x%1").arg(targetAddress, 2, 16, QChar('0')).toUpper();
    
    // Send Product Information request
    requestProductInformation(targetAddress);
    
    // Schedule Configuration Information request after a short delay
    QTimer::singleShot(500, [this, targetAddress]() {
        queryDeviceConfiguration(targetAddress);
    });
    
    // Schedule Supported PGNs request after another delay
    QTimer::singleShot(1000, [this, targetAddress]() {
        requestSupportedPGNs(targetAddress);
    });
    
    qDebug() << "Scheduled all information requests for device"
             << QString("0x%1").arg(targetAddress, 2, 16, QChar('0')).toUpper()
             << "- monitor device table and PGN log for responses";
}

void DeviceMainWindow::requestInfoFromAllDevices()
{
    if (!nmea2000) {
        qDebug() << "Cannot request information from all devices: NMEA2000 interface not available";
        return;
    }
    
    qDebug() << "Requesting information from all devices";
    
    int confirmedDevices = 0;
    
    // Count devices that have responded to anything
    for (int row = 0; row < m_deviceTable->rowCount(); ++row) {
        QTableWidgetItem* sourceItem = m_deviceTable->item(row, 0);
        if (sourceItem) {
            uint8_t sourceAddress = sourceItem->text().toUInt();
            
            // Check if this device has shown any activity or has any known information
            if (m_deviceActivity.contains(sourceAddress) && m_deviceActivity[sourceAddress].isActive) {
                confirmedDevices++;
            }
        }
    }
    
    if (confirmedDevices == 0) {
        qDebug() << "No active devices found for information requests";
        return;
    }
    
    qDebug() << "Requesting information from" << confirmedDevices << "active device(s)";
    
    // Request information from all active devices
    int requestsSent = 0;
    for (int row = 0; row < m_deviceTable->rowCount(); ++row) {
        QTableWidgetItem* sourceItem = m_deviceTable->item(row, 0);
        if (sourceItem) {
            uint8_t sourceAddress = sourceItem->text().toUInt();
            
            // Only request from devices that are currently active
            if (m_deviceActivity.contains(sourceAddress) && m_deviceActivity[sourceAddress].isActive) {
                // Send Product Information request
                requestProductInformation(sourceAddress);
                
                // Schedule Configuration Information request with delay based on device index
                int delay = (requestsSent * 1500) + 500; // Stagger requests
                QTimer::singleShot(delay, [this, sourceAddress]() {
                    queryDeviceConfiguration(sourceAddress);
                });
                
                // Schedule Supported PGNs request with additional delay
                QTimer::singleShot(delay + 500, [this, sourceAddress]() {
                    requestSupportedPGNs(sourceAddress);
                });
                
                requestsSent++;
            }
        }
    }
    
    if (requestsSent > 0) {
        statusBar()->showMessage(QString("Sent information requests to %1 device(s) - responses will appear over the next few seconds").arg(requestsSent), 10000);
        qDebug() << "Sent information requests to" << requestsSent << "device(s)";
    }
}

void DeviceMainWindow::sendInitialBroadcastRequest()
{
    if (!nmea2000) {
        qDebug() << "Cannot send initial broadcast request - NMEA2000 interface not available";
        return;
    }
    
    qDebug() << "Sending initial broadcast ISO request for Product Information (PGN 126996)";
    
    // Send broadcast ISO request for Product Information to wake up all devices
    tN2kMsg msg;
    SetN2kPGN59904(msg, 0xFF, N2kPGNProductInformation); // 0xFF = broadcast
    
    if (nmea2000->SendMsg(msg)) {
        // Blink TX indicator for transmitted message
        blinkTxIndicator();
        
        qDebug() << "Initial broadcast request sent successfully";
        statusBar()->showMessage("Sent initial broadcast request for device discovery...", 3000);
    } else {
        qDebug() << "Failed to send initial broadcast request";
    }
}

void DeviceMainWindow::triggerAutomaticDeviceDiscovery()
{
    if (!nmea2000) {
        qDebug() << "Cannot trigger automatic discovery - NMEA2000 interface not available";
        return;
    }
    
    qDebug() << "Triggering automatic device discovery - sending wake-up broadcast";
    
    // Count current devices
    int currentDevices = m_deviceTable->rowCount();
    
    statusBar()->showMessage("Sending network wake-up broadcast to discover quiet devices...", 5000);
    
    // Send a single "wake-up" broadcast request for Product Information
    // This serves as a network knock to get quiet devices to identify themselves
    // Individual enumeration and follow-up queries will handle detailed information gathering
    if (m_deviceList) {
        qDebug() << "Sending wake-up broadcast for Product Information to discover quiet devices";
        tN2kMsg msg;
        SetN2kPGN59904(msg, 0xFF, N2kPGNProductInformation); // 0xFF = broadcast
        nmea2000->SendMsg(msg);
        
        // Blink TX indicator for transmitted messages
        blinkTxIndicator();
    }
    
    // Show completion message after a short delay to let responses come in
    QTimer::singleShot(2000, [this, currentDevices]() {
        int newDevices = m_deviceTable->rowCount();
        QString message;
        if (newDevices > currentDevices) {
            message = QString("Wake-up broadcast completed. %1 new device(s) responded and will be queried individually")
                     .arg(newDevices - currentDevices);
        } else {
            message = "Wake-up broadcast completed. Individual device queries will handle information gathering.";
        }
        
        statusBar()->showMessage(message, 5000);
        qDebug() << message;
    });
}

void DeviceMainWindow::scheduleFollowUpQueries()
{
    if (m_followUpQueriesScheduled) {
        return; // Already scheduled
    }
    
    m_followUpQueriesScheduled = true;
    qDebug() << "Scheduling follow-up queries for devices with missing information in" << FOLLOWUP_QUERY_DELAY_MS << "ms";
    
    // Schedule follow-up queries after the initial discovery has had time to complete
    QTimer::singleShot(FOLLOWUP_QUERY_DELAY_MS, this, &DeviceMainWindow::performFollowUpQueries);
}

void DeviceMainWindow::performFollowUpQueries()
{
    if (!nmea2000 || !m_autoDiscoveryCheckbox || !m_autoDiscoveryCheckbox->isChecked()) {
        qDebug() << "Skipping follow-up queries - interface or auto-discovery not available";
        return;
    }
    
    qDebug() << "Performing follow-up queries for devices with missing information";
    
    QStringList devicesWithMissingInfo;
    int queriesSent = 0;
    
    // Check each device in the table for missing information
    for (int row = 0; row < m_deviceTable->rowCount(); ++row) {
        QTableWidgetItem* sourceItem = m_deviceTable->item(row, 0);
        if (!sourceItem) continue;
        
        uint8_t sourceAddress = sourceItem->text().toUInt();
        
        // Check if this device is still active
        if (!m_deviceActivity.contains(sourceAddress) || !m_deviceActivity[sourceAddress].isActive) {
            continue;
        }
        
        // Check for missing critical information: Manufacturer (column 1), Model ID (column 2) and Serial Number (column 3)
        QTableWidgetItem* manufacturerItem = m_deviceTable->item(row, 1);
        QTableWidgetItem* modelItem = m_deviceTable->item(row, 2);
        QTableWidgetItem* serialItem = m_deviceTable->item(row, 3);
        
        bool needsQuery = false;
        QString deviceDesc = QString("0x%1").arg(sourceAddress, 2, 16, QChar('0')).toUpper();
        QStringList missingFields;
        
        if (!manufacturerItem || manufacturerItem->text().isEmpty() || 
            manufacturerItem->text() == "Unknown" || manufacturerItem->text().contains("Unknown (")) {
            needsQuery = true;
            missingFields << "Manufacturer";
        }
        
        if (!modelItem || modelItem->text().isEmpty() || modelItem->text() == "Unknown") {
            needsQuery = true;
            missingFields << "Model ID";
        }
        
        if (!serialItem || serialItem->text().isEmpty() || serialItem->text() == "Unknown") {
            needsQuery = true;
            missingFields << "Serial Number";
        }
        
        if (needsQuery) {
            deviceDesc += QString(" (missing %1)").arg(missingFields.join(", "));
            devicesWithMissingInfo << deviceDesc;
            
            // Send targeted requests with delays to avoid network flooding
            int delay = queriesSent * 800; // 800ms between devices
            
            QTimer::singleShot(delay, [this, sourceAddress]() {
                if (nmea2000) {
                    qDebug() << "Sending follow-up Product Information request to device" << 
                               QString("0x%1").arg(sourceAddress, 2, 16, QChar('0')).toUpper();
                    requestProductInformation(sourceAddress);
                }
            });
            
            QTimer::singleShot(delay + 400, [this, sourceAddress]() {
                if (nmea2000) {
                    qDebug() << "Sending follow-up Configuration Information request to device" << 
                               QString("0x%1").arg(sourceAddress, 2, 16, QChar('0')).toUpper();
                    queryDeviceConfiguration(sourceAddress);
                }
            });
            
            queriesSent++;
        }
    }
    
    if (queriesSent > 0) {
        QString statusMessage = QString("Requesting missing information from %1 device(s)").arg(queriesSent);
        statusBar()->showMessage(statusMessage, 8000);
        qDebug() << "Follow-up queries sent to" << queriesSent << "devices:" << devicesWithMissingInfo;
        
        // Show completion message
        int totalDelay = (queriesSent * 800) + 2000;
        QTimer::singleShot(totalDelay, [this, queriesSent]() {
            QString message = QString("Follow-up information requests completed for %1 device(s)").arg(queriesSent);
            statusBar()->showMessage(message, 5000);
            qDebug() << message;
        });
    } else {
        qDebug() << "No devices found with missing critical information";
    }
}

void DeviceMainWindow::queryNewDevice(uint8_t sourceAddress)
{
    if (!nmea2000 || !m_autoDiscoveryCheckbox || !m_autoDiscoveryCheckbox->isChecked()) {
        qDebug() << "Skipping new device query - interface or auto-discovery not available";
        return;
    }
    
    qDebug() << "Querying new device" << QString("0x%1").arg(sourceAddress, 2, 16, QChar('0')).toUpper() 
             << "for device information";
    
    // Check if device is still active before querying
    if (!m_deviceActivity.contains(sourceAddress) || !m_deviceActivity[sourceAddress].isActive) {
        qDebug() << "Device" << QString("0x%1").arg(sourceAddress, 2, 16, QChar('0')).toUpper() 
                 << "is no longer active, skipping query";
        return;
    }
    
    QString deviceDesc = QString("0x%1").arg(sourceAddress, 2, 16, QChar('0')).toUpper();
    statusBar()->showMessage(QString("Requesting information from new device %1").arg(deviceDesc), 5000);
    
    // Send Product Information request
    requestProductInformation(sourceAddress);
    
    // Send Configuration Information request with a delay
    QTimer::singleShot(500, [this, sourceAddress]() {
        if (nmea2000 && m_deviceActivity.contains(sourceAddress) && m_deviceActivity[sourceAddress].isActive) {
            queryDeviceConfiguration(sourceAddress);
        }
    });
    
    // Send Supported PGNs request with additional delay
    QTimer::singleShot(1000, [this, sourceAddress]() {
        if (nmea2000 && m_deviceActivity.contains(sourceAddress) && m_deviceActivity[sourceAddress].isActive) {
            requestSupportedPGNs(sourceAddress);
        }
    });
    
    qDebug() << "Information requests sent to new device" << deviceDesc;
}

void DeviceMainWindow::showPGNLogForDevice(uint8_t sourceAddress)
{
    // Create a new PGN log dialog for this specific device
    PGNLogDialog* deviceDialog = new PGNLogDialog(this);
    
    // Set up device name resolver
    deviceDialog->setDeviceNameResolver([this](uint8_t address) {
        return getDeviceName(address);
    });
    
    // Connect the destroyed signal to our cleanup slot
    connect(deviceDialog, &QObject::destroyed, this, &DeviceMainWindow::onPGNLogDialogDestroyed);
    
    // Add to our list of dialogs
    m_pgnLogDialogs.append(deviceDialog);
    
    // Ensure the device list is up to date before setting filters
    updatePGNDialogDeviceList();
    
    // Set filters for both source and destination addresses to capture all traffic
    // involving this device (messages from it OR messages to it)
    deviceDialog->setSourceFilter(sourceAddress);
    deviceDialog->setDestinationFilter(sourceAddress);
    deviceDialog->setFilterLogic(true); // Use OR logic - show messages FROM device OR TO device
    
    deviceDialog->setWindowTitle(QString("PGN History - Device 0x%1 (Source OR Destination)")
                                .arg(sourceAddress, 2, 16, QChar('0')).toUpper());
    
    deviceDialog->show();
    deviceDialog->raise();
    deviceDialog->activateWindow();
    
    qDebug() << "Created device-specific PGN dialog for device 0x" 
             << QString::number(sourceAddress, 16).toUpper() 
             << ". Total dialogs: " << m_pgnLogDialogs.size();
}

void DeviceMainWindow::clearConflictHistory()
{
    m_conflictAnalyzer->clearHistory();
    updateDeviceList(); // Refresh display
}

bool DeviceMainWindow::hasInstanceConflicts() const {
    return m_conflictAnalyzer->hasConflicts();
}

int DeviceMainWindow::getConflictCount() const {
    return m_conflictAnalyzer->getConflictCount();
}

void DeviceMainWindow::analyzeInstanceConflicts() {
    m_conflictAnalyzer->analyzeAndShowConflicts();
}

QString DeviceMainWindow::getDeviceClassName(unsigned char deviceClass) {
    return QString("Device Class %1").arg(deviceClass);
}

QString DeviceMainWindow::getDeviceFunctionName(unsigned char deviceFunction) {
    return QString("Function %1").arg(deviceFunction);
}

QString DeviceMainWindow::getManufacturerName(uint16_t manufacturerCode) const {
    switch(manufacturerCode) {
        case 126:  return "Furuno";
        case 130:  return "Raymarine";
        case 135:  return "Airmar";
        case 137:  return "Maretron";
        case 147:  return "Garmin";
        case 165:  return "B&G";
        case 176:  return "Carling Technologies";
        case 194:  return "Simrad";
        case 229:  return "Garmin"; // Furuno is also 229, but Garmin is more common
        case 304:  return "EmpirBus"; // Added manufacturer
        case 358:  return "Victron";
        case 504:  return "Vesper";
        case 1084: return "ShadowCaster";
        case 1403: return "Arco";
        case 1440: return "Egis Mobile";
        case 1512: return "Lumitec";
        case 1857: return "Simrad";
        default:   return QString("Unknown (%1)").arg(manufacturerCode);
    }
}

QString DeviceMainWindow::getPGNName(unsigned long pgn) {
    switch(pgn) {
        case 127488: return "Engine Parameters, Rapid";
        case 127489: return "Engine Parameters, Dynamic";
        case 127502: return "Binary Switch Bank Control";
        case 127505: return "Fluid Level";
        case 127508: return "Battery Status";
        case 127509: return "Inverter Status";
        case 127513: return "Battery Configuration Status";
        case 128259: return "Speed";
        case 128267: return "Water Depth";
        case 129025: return "Position, Rapid Update";
        case 129026: return "COG & SOG, Rapid Update";
        case 129029: return "GNSS Position Data";
        case 130306: return "Wind Data";
        case 130312: return "Temperature";
        case 130314: return "Actual Pressure";
        case 130316: return "Temperature, Extended Range";
        case 61184: return "Lumitec Poco Proprietary";
        default: return QString("PGN %1").arg(pgn);
    }
}

QString DeviceMainWindow::getDeviceName(uint8_t deviceAddress) const {
    // Search the device table for the given address
    for (int row = 0; row < m_deviceTable->rowCount(); row++) {
        QTableWidgetItem* nodeItem = m_deviceTable->item(row, 0);
        if (nodeItem && nodeItem->text().toUInt(nullptr, 16) == deviceAddress) {
            QTableWidgetItem* nameItem = m_deviceTable->item(row, 1);
            if (nameItem && !nameItem->text().isEmpty()) {
                return nameItem->text();
            }
            break;
        }
    }
    
    // Return address if device name not found
    return QString("0x%1").arg(deviceAddress, 2, 16, QChar('0'));
}

// Lumitec Poco Message Handling
void DeviceMainWindow::handleLumitecPocoMessage(const tN2kMsg& msg) {
    uint8_t proprietaryId;
    if (!ParseLumitecPGN61184(msg, proprietaryId)) {
        return; // Not a valid Lumitec message
    }
    
    QString description;
    
    switch (proprietaryId) {
        case PID_EXTSW_SIMPLE_ACTIONS: {
            LumitecExtSwSimpleAction action;
            if (ParseLumitecExtSwSimpleAction(msg, action)) {
                description = QString("ExtSw Simple Action - Switch %1: %2")
                             .arg(action.switchId)
                             .arg(GetLumitecActionName(action.actionId));
            }
            break;
        }
        
        case PID_EXTSW_STATE_INFO: {
            LumitecExtSwStateInfo stateInfo;
            if (ParseLumitecExtSwStateInfo(msg, stateInfo)) {
                description = QString("ExtSw State - Switch %1: State=%2, Type=%3")
                             .arg(stateInfo.extSwId)
                             .arg(stateInfo.extSwState)
                             .arg(GetLumitecExtSwTypeName(stateInfo.extSwType));
            }
            break;
        }
        
        case PID_EXTSW_CUSTOM_HSB: {
            LumitecExtSwCustomHSB customHSB;
            if (ParseLumitecExtSwCustomHSB(msg, customHSB)) {
                description = QString("ExtSw Custom HSB - Switch %1: %2 H=%3 S=%4 B=%5")
                             .arg(customHSB.switchId)
                             .arg(GetLumitecActionName(customHSB.actionId))
                             .arg(customHSB.hue)
                             .arg(customHSB.saturation)
                             .arg(customHSB.brightness);
            }
            break;
        }
        
        case PID_EXTSW_START_PATTERN: {
            LumitecExtSwStartPattern startPattern;
            if (ParseLumitecExtSwStartPattern(msg, startPattern)) {
                description = QString("ExtSw Start Pattern - Switch %1: Pattern %2")
                             .arg(startPattern.switchId)
                             .arg(startPattern.patternId);
            }
            break;
        }
        
        case PID_OUTPUT_CHANNEL_STATUS: {
            LumitecOutputChannelStatus status;
            if (ParseLumitecOutputChannelStatus(msg, status)) {
                double voltage = status.inputVoltage * 0.2; // 200mV units
                double current = status.current * 0.1;      // 100mA units
                description = QString("Output Channel %1 Status - Mode: %2, Level: %3, %.1fV, %.1fA")
                             .arg(status.channel)
                             .arg(GetLumitecChannelModeName(status.channelMode))
                             .arg(status.outputLevel)
                             .arg(voltage)
                             .arg(current);
            }
            break;
        }
        
        case PID_OUTPUT_CHANNEL_BIN: {
            LumitecOutputChannelBin binControl;
            if (ParseLumitecOutputChannelBin(msg, binControl)) {
                description = QString("Output Channel %1 Binary - %2")
                             .arg(binControl.channel)
                             .arg(binControl.state ? "ON" : "OFF");
            }
            break;
        }
        
        case PID_OUTPUT_CHANNEL_PWM: {
            LumitecOutputChannelPWM pwmControl;
            if (ParseLumitecOutputChannelPWM(msg, pwmControl)) {
                double duty = (pwmControl.duty / 255.0) * 100.0; // Convert to percentage
                description = QString("Output Channel %1 PWM - Duty: %.1f%%, Transition: %2ms")
                             .arg(pwmControl.channel)
                             .arg(duty)
                             .arg(pwmControl.transitionTime);
            }
            break;
        }
        
        case PID_OUTPUT_CHANNEL_PLI: {
            LumitecOutputChannelPLI pliControl;
            if (ParseLumitecOutputChannelPLI(msg, pliControl)) {
                description = QString("Output Channel %1 PLI - Message: 0x%2")
                             .arg(pliControl.channel)
                             .arg(pliControl.pliMessage, 8, 16, QChar('0'));
            }
            break;
        }
        
        case PID_OUTPUT_CHANNEL_PLI_T2HSB: {
            LumitecOutputChannelPLIT2HSB pliT2HSB;
            if (ParseLumitecOutputChannelPLIT2HSB(msg, pliT2HSB)) {
                description = QString("Output Channel %1 PLI T2HSB - Clan:%2 Trans:%3 H=%4 S=%5 B=%6")
                             .arg(pliT2HSB.channel)
                             .arg(pliT2HSB.pliClan)
                             .arg(pliT2HSB.transition)
                             .arg(pliT2HSB.hue)
                             .arg(pliT2HSB.saturation)
                             .arg(pliT2HSB.brightness);
            }
            break;
        }
        
        default:
            description = QString("Unknown Lumitec PID %1").arg(proprietaryId);
            break;
    }
    
    if (!description.isEmpty()) {
        displayLumitecMessage(msg, description);
    }
}

// Product Information Message Handling
void DeviceMainWindow::handleProductInformationResponse(const tN2kMsg& msg) {
    if (msg.PGN != N2kPGNProductInformation) {
        return;
    }
    
    // Check if this is a response to a request we made
    bool showDialog = m_pendingProductInfoRequests.contains(msg.Source);
    if (showDialog) {
        // Remove from pending requests since we got the response
        m_pendingProductInfoRequests.remove(msg.Source);
        
        // Cancel any retry timer for this device
        cancelProductInfoRetry(msg.Source);
        
        // Reset retry count for this device
        m_productInfoRetryCount.remove(msg.Source);
    }
    
    // Basic parsing of Product Information (PGN 126996)
    if (msg.DataLen < 10) { // Minimum expected length
        qDebug() << "Product Information message too short: " << msg.DataLen << " bytes";
        return;
    }
    
    uint16_t N2kVersion = 0;
    uint16_t ProductCode = 0;
    char ModelID[33] = {0}; // Max 32 chars + null terminator
    char SoftwareVersion[33] = {0};
    char ModelVersion[33] = {0};
    char SerialCode[33] = {0};
    uint8_t CertificationLevel = 0;
    uint8_t LoadEquivalency = 0;
    
    // Use the official NMEA2000 parsing function for proper string handling
    if (ParseN2kPGN126996(msg, N2kVersion, ProductCode,
                          sizeof(ModelID), ModelID,
                          sizeof(SoftwareVersion), SoftwareVersion,
                          sizeof(ModelVersion), ModelVersion,
                          sizeof(SerialCode), SerialCode,
                          CertificationLevel, LoadEquivalency)) {
        
        //qDebug() << "Received Product Information from Device"
        //         << QString("0x%1:").arg(msg.Source, 2, 16, QChar('0')).toUpper()
        //         << "Model ID:" << QString::fromLatin1(ModelID)
        //         << "Serial:" << QString::fromLatin1(SerialCode)
        //         << "Software:" << QString::fromLatin1(SoftwareVersion);
        
        // Update device table to reflect the new information
        populateDeviceTable();
        
        if (showDialog) {
            // Log detailed response for explicit requests
            qDebug() << "Product Information Response from Device"
                     << QString("0x%1:").arg(msg.Source, 2, 16, QChar('0'))
                     << "N2K Version:" << N2kVersion
                     << "Product Code:" << ProductCode
                     << "Model ID:" << QString::fromLatin1(ModelID)
                     << "Software Version:" << QString::fromLatin1(SoftwareVersion)
                     << "Model Version:" << QString::fromLatin1(ModelVersion)
                     << "Serial Code:" << QString::fromLatin1(SerialCode);
        }
    }
    // Note: Unsolicited product information is silently ignored
}

// Configuration Information Message Handling (PGN 126998)
void DeviceMainWindow::handleConfigurationInformationResponse(const tN2kMsg& msg) {
    if (msg.PGN != N2kPGNConfigurationInformation) {
        return;
    }
    
    // Check if this is a response to a request we made
    bool showDialog = m_pendingConfigInfoRequests.contains(msg.Source);
    if (showDialog) {
        // Remove from pending requests since we got the response
        m_pendingConfigInfoRequests.remove(msg.Source);
    }
    
    char ManufacturerInformation[71] = {0}; // Max 70 chars + null terminator
    char InstallationDescription1[71] = {0};
    char InstallationDescription2[71] = {0};
    size_t ManufacturerInformationSize = sizeof(ManufacturerInformation);
    size_t InstallationDescription1Size = sizeof(InstallationDescription1);
    size_t InstallationDescription2Size = sizeof(InstallationDescription2);
    
    // Use the official NMEA2000 parsing function
    if (ParseN2kPGN126998(msg, 
                          ManufacturerInformationSize, ManufacturerInformation,
                          InstallationDescription1Size, InstallationDescription1,
                          InstallationDescription2Size, InstallationDescription2)) {
        
        // Helper function to decode NMEA2000 strings (which can be ASCII or Unicode with SOH prefix)
        auto decodeN2kString = [](const char* rawStr) -> QString {
            if (!rawStr || rawStr[0] == '\0') {
                return QString("(empty)");
            }
            
            // Check if string starts with SOH (Start of Header, 0x01) indicating Unicode
            if (rawStr[0] == '\x01') {
                // Unicode string - skip SOH and interpret as UTF-8
                return QString::fromUtf8(rawStr + 1);
            } else {
                // ASCII string
                return QString::fromLatin1(rawStr);
            }
        };
        
        QString decodedMfg = decodeN2kString(ManufacturerInformation);
        QString decodedDesc1 = decodeN2kString(InstallationDescription1);
        QString decodedDesc2 = decodeN2kString(InstallationDescription2);
        
        // Update device table to reflect the new information
        populateDeviceTable();
    }
}

// Group Function Message Handling (PGN 126208)
void DeviceMainWindow::handleGroupFunctionMessage(const tN2kMsg& msg) {
    if (msg.PGN != 126208UL) {
        return;
    }
    
    tN2kGroupFunctionCode GroupFunctionCode;
    unsigned long PGNForGroupFunction;
    
    // Use the official NMEA2000 parsing function
    if (tN2kGroupFunctionHandler::Parse(msg, GroupFunctionCode, PGNForGroupFunction)) {
        // Check if this is an acknowledgment
        if (GroupFunctionCode == N2kgfc_Acknowledge) {
            qDebug() << "Received Group Function ACK from device" << QString("0x%1").arg(msg.Source, 2, 16, QChar('0'))
                     << "for PGN" << PGNForGroupFunction;
            
            // Emit signal to notify waiting dialogs
            emit commandAcknowledged(msg.Source, PGNForGroupFunction, true);
        }
    }
}

// Lumitec Poco Control Methods
void DeviceMainWindow::sendLumitecSimpleAction(uint8_t targetAddress, uint8_t actionId, uint8_t switchId) {
    if (!nmea2000) {
        qDebug() << "NMEA2000 not initialized, cannot send Lumitec message";
        return;
    }
    
    tN2kMsg msg;
    if (SetLumitecExtSwSimpleAction(msg, targetAddress, actionId, switchId)) {
        if (nmea2000->SendMsg(msg)) {
            // Blink TX indicator for transmitted messages
            blinkTxIndicator();
            
            // Log the sent message to all PGN log dialogs
            for (PGNLogDialog* dialog : m_pgnLogDialogs) {
                if (dialog && dialog->isVisible()) {
                    dialog->appendSentMessage(msg);
                }
            }
            
            qDebug() << "Sent Lumitec Simple Action - Target:" << QString("0x%1").arg(targetAddress, 2, 16, QChar('0'))
                     << "Action:" << GetLumitecActionName(actionId) 
                     << "Switch:" << switchId;
        } else {
            qDebug() << "Failed to send Lumitec Simple Action message";
        }
    } else {
        qDebug() << "Failed to create Lumitec Simple Action message";
    }
}

void DeviceMainWindow::showLumitecColorDialog(uint8_t targetAddress, const QString& nodeAddress) {
    // Create a simple color dialog for HSB control
    QDialog dialog(this);
    dialog.setWindowTitle(QString("Lumitec Color Control - Device 0x%1").arg(nodeAddress));
    dialog.setModal(true);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    // Hue slider
    layout->addWidget(new QLabel("Hue (0-255):"));
    QSlider* hueSlider = new QSlider(Qt::Horizontal);
    hueSlider->setRange(0, 255);
    hueSlider->setValue(128);
    QLabel* hueLabel = new QLabel("128");
    connect(hueSlider, &QSlider::valueChanged, [hueLabel](int value) {
        hueLabel->setText(QString::number(value));
    });
    QHBoxLayout* hueLayout = new QHBoxLayout();
    hueLayout->addWidget(hueSlider);
    hueLayout->addWidget(hueLabel);
    layout->addLayout(hueLayout);
    
    // Saturation slider
    layout->addWidget(new QLabel("Saturation (0-255):"));
    QSlider* satSlider = new QSlider(Qt::Horizontal);
    satSlider->setRange(0, 255);
    satSlider->setValue(255);
    QLabel* satLabel = new QLabel("255");
    connect(satSlider, &QSlider::valueChanged, [satLabel](int value) {
        satLabel->setText(QString::number(value));
    });
    QHBoxLayout* satLayout = new QHBoxLayout();
    satLayout->addWidget(satSlider);
    satLayout->addWidget(satLabel);
    layout->addLayout(satLayout);
    
    // Brightness slider
    layout->addWidget(new QLabel("Brightness (0-255):"));
    QSlider* brightSlider = new QSlider(Qt::Horizontal);
    brightSlider->setRange(0, 255);
    brightSlider->setValue(255);
    QLabel* brightLabel = new QLabel("255");
    connect(brightSlider, &QSlider::valueChanged, [brightLabel](int value) {
        brightLabel->setText(QString::number(value));
    });
    QHBoxLayout* brightLayout = new QHBoxLayout();
    brightLayout->addWidget(brightSlider);
    brightLayout->addWidget(brightLabel);
    layout->addLayout(brightLayout);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* sendButton = new QPushButton("Send Custom HSB");
    QPushButton* cancelButton = new QPushButton("Cancel");
    buttonLayout->addWidget(sendButton);
    buttonLayout->addWidget(cancelButton);
    layout->addLayout(buttonLayout);
    
    connect(sendButton, &QPushButton::clicked, [this, targetAddress, hueSlider, satSlider, brightSlider, &dialog]() {
        sendLumitecCustomHSB(targetAddress, hueSlider->value(), satSlider->value(), brightSlider->value());
        dialog.accept();
    });
    
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    
    dialog.exec();
}

void DeviceMainWindow::sendLumitecCustomHSB(uint8_t targetAddress, uint8_t hue, uint8_t saturation, uint8_t brightness) {
    if (!nmea2000) {
        qDebug() << "NMEA2000 not initialized, cannot send Lumitec message";
        return;
    }
    
    tN2kMsg msg;
    if (SetLumitecExtSwCustomHSB(msg, targetAddress, ACTION_T2HSB, 1, hue, saturation, brightness)) {
        if (nmea2000->SendMsg(msg)) {
            // Blink TX indicator for transmitted messages
            blinkTxIndicator();
            
            // Log the sent message to all PGN log dialogs
            for (PGNLogDialog* dialog : m_pgnLogDialogs) {
                if (dialog && dialog->isVisible()) {
                    dialog->appendSentMessage(msg);
                }
            }
            
            qDebug() << "Sent Lumitec Custom HSB - Target:" << QString("0x%1").arg(targetAddress, 2, 16, QChar('0'))
                     << "H:" << hue << "S:" << saturation << "B:" << brightness;
        } else {
            qDebug() << "Failed to send Lumitec Custom HSB message";
        }
    } else {
        qDebug() << "Failed to create Lumitec Custom HSB message";
    }
}

void DeviceMainWindow::showPocoDeviceDialog(uint8_t targetAddress, const QString& nodeAddress) {
    PocoDeviceDialog* dialog = new PocoDeviceDialog(targetAddress, nodeAddress, this);
    
    // Connect signals from the dialog to our slots
    connect(dialog, &PocoDeviceDialog::switchActionRequested,
            this, &DeviceMainWindow::onPocoSwitchActionRequested);
    connect(dialog, &PocoDeviceDialog::colorControlRequested,
            this, &DeviceMainWindow::onPocoColorControlRequested);
    connect(dialog, &PocoDeviceDialog::deviceInfoRequested,
            this, &DeviceMainWindow::onPocoDeviceInfoRequested);
    
    // Show the dialog non-modally
    dialog->show();
    
    // Set up automatic cleanup when dialog is closed
    connect(dialog, &QDialog::finished, dialog, &QObject::deleteLater);
}

void DeviceMainWindow::onPocoSwitchActionRequested(uint8_t deviceAddress, uint8_t switchId, uint8_t actionId) {
    sendLumitecSimpleAction(deviceAddress, actionId, switchId);
}

void DeviceMainWindow::onZonePGN130561Requested(uint8_t deviceAddress, uint8_t zoneId, const QString& zoneName,
                                                uint8_t red, uint8_t green, uint8_t blue, uint16_t colorTemp,
                                                uint8_t intensity, uint8_t programId, uint8_t programColorSeqIndex,
                                                uint8_t programIntensity, uint8_t programRate,
                                                uint8_t programColorSequence, bool zoneEnabled) {
    sendZonePGN130561(deviceAddress, zoneId, zoneName, red, green, blue, colorTemp,
                      intensity, programId, programColorSeqIndex, programIntensity,
                      programRate, programColorSequence, zoneEnabled);
}

void DeviceMainWindow::onPocoZoneLightingControlRequested(uint8_t deviceAddress) {
    QString nodeAddress = QString("0x%1").arg(deviceAddress, 2, 16, QChar('0'));
    
    // Find device name from the device table
    QString deviceName = "Unknown Device";
    for (int row = 0; row < m_deviceTable->rowCount(); row++) {
        QTableWidgetItem* nodeItem = m_deviceTable->item(row, 0);
        if (nodeItem && nodeItem->text().toUInt(nullptr, 16) == deviceAddress) {
            QTableWidgetItem* nameItem = m_deviceTable->item(row, 1);
            if (nameItem) {
                deviceName = nameItem->text();
            }
            break;
        }
    }
    
    ZoneLightingDialog* zoneLightingDialog = new ZoneLightingDialog(deviceAddress, deviceName, this);
    
    // Connect the zone lighting dialog signal
    connect(zoneLightingDialog, &ZoneLightingDialog::zonePGN130561Requested,
            this, &DeviceMainWindow::onZonePGN130561Requested);
    
    // Connect acknowledgment signal to allow waiting for ACKs
    connect(this, &DeviceMainWindow::commandAcknowledged,
            zoneLightingDialog, &ZoneLightingDialog::onCommandAcknowledged);
    
    // Show the dialog non-modally
    zoneLightingDialog->show();
    
    // Set up automatic cleanup when dialog is closed
    connect(zoneLightingDialog, &QDialog::finished, zoneLightingDialog, &QObject::deleteLater);
}

void DeviceMainWindow::onPocoColorControlRequested(uint8_t deviceAddress) {
    // For now, delegate to the existing color dialog
    QString nodeAddress = QString("0x%1").arg(deviceAddress, 2, 16, QChar('0'));
    showLumitecColorDialog(deviceAddress, nodeAddress);
}

void DeviceMainWindow::onPocoDeviceInfoRequested(uint8_t deviceAddress) {
    // Request product information and device configuration
    requestProductInformation(deviceAddress);
    queryDeviceConfiguration(deviceAddress);
}

// Device Activity Tracking Methods
void DeviceMainWindow::updateDeviceActivity(uint8_t sourceAddress) {
    QDateTime now = QDateTime::currentDateTime();
    
    if (m_deviceActivity.contains(sourceAddress)) {
        m_deviceActivity[sourceAddress].lastSeen = now;
        m_deviceActivity[sourceAddress].isActive = true;
    } else {
        DeviceActivity activity;
        activity.lastSeen = now;
        activity.isActive = true;
        activity.tableRow = -1; // Will be set when device is added to table
        m_deviceActivity[sourceAddress] = activity;
    }
}

void DeviceMainWindow::checkDeviceTimeouts() {
    QDateTime now = QDateTime::currentDateTime();
    QList<uint8_t> devicesToRemove;
    
    for (auto it = m_deviceActivity.begin(); it != m_deviceActivity.end(); ++it) {
        qint64 msSinceLastSeen = it->lastSeen.msecsTo(now);
        
        if (msSinceLastSeen > DEVICE_REMOVAL_TIMEOUT_MS) {
            // Device has been inactive for removal timeout - mark for removal
            devicesToRemove.append(it.key());
        } else if (msSinceLastSeen > DEVICE_TIMEOUT_MS) {
            // Device has been inactive for timeout - mark as inactive but don't remove yet
            it->isActive = false;
        }
    }
    
    // Remove devices that have been inactive too long
    for (uint8_t deviceAddress : devicesToRemove) {
        removeInactiveDevice(deviceAddress);
    }
}

void DeviceMainWindow::removeInactiveDevice(uint8_t deviceAddress) {
    // Find and remove the device from the table
    for (int row = 0; row < m_deviceTable->rowCount(); ++row) {
        QTableWidgetItem* item = m_deviceTable->item(row, 0);
        if (item) {
            bool ok;
            uint8_t tableAddress = item->text().toUInt(&ok, 16);
            if (ok && tableAddress == deviceAddress) {
                m_deviceTable->removeRow(row);
                qDebug() << "Removed inactive device" << QString("0x%1").arg(deviceAddress, 2, 16, QChar('0')).toUpper() 
                         << "from device table after timeout";
                break;
            }
        }
    }
    
    // Remove from activity tracking
    m_deviceActivity.remove(deviceAddress);
    
    // Remove from known devices (so it can be rediscovered if it comes back)
    m_knownDevices.remove(deviceAddress);
    
    // Cancel any pending product info requests/timers for this device
    if (m_productInfoRetryTimers.contains(deviceAddress)) {
        QTimer* timer = m_productInfoRetryTimers[deviceAddress];
        if (timer) {
            timer->stop();
            timer->deleteLater();
        }
        m_productInfoRetryTimers.remove(deviceAddress);
    }
    m_pendingProductInfoRequests.remove(deviceAddress);
    m_pendingConfigInfoRequests.remove(deviceAddress);
    m_productInfoRetryCount.remove(deviceAddress);
    
    // Update table row indices for remaining devices
    for (auto it = m_deviceActivity.begin(); it != m_deviceActivity.end(); ++it) {
        // Reset table row indices - they'll be updated on next population
        it->tableRow = -1;
    }
}

void DeviceMainWindow::grayOutInactiveDevices() {
    for (auto it = m_deviceActivity.begin(); it != m_deviceActivity.end(); ++it) {
        const DeviceActivity& activity = it.value();
        
        if (activity.tableRow >= 0 && activity.tableRow < m_deviceTable->rowCount()) {
            QColor textColor = activity.isActive ? QColor(Qt::black) : QColor(Qt::gray);
            
            for (int col = 0; col < m_deviceTable->columnCount(); col++) {
                QTableWidgetItem* item = m_deviceTable->item(activity.tableRow, col);
                if (item) {
                    item->setForeground(QBrush(textColor));
                }
            }
        }
    }
}

void DeviceMainWindow::updateDeviceTableRow(int row, uint8_t source, const tNMEA2000::tDevice* device, bool isActive) {
    // Node Address (Source) - in hex format like Maretron
    QTableWidgetItem* nodeAddressItem = new QTableWidgetItem(QString("%1").arg(source, 2, 16, QChar('0')).toUpper());
    nodeAddressItem->setTextAlignment(Qt::AlignCenter);
    m_deviceTable->setItem(row, 0, nodeAddressItem);
    
    // Manufacturer - convert manufacturer code to name
    uint16_t manufacturerCode = device->GetManufacturerCode();
    QString manufacturerName = getManufacturerName(manufacturerCode);
    m_deviceTable->setItem(row, 1, new QTableWidgetItem(manufacturerName));
    
    // Mfg Model ID - use virtual method
    QString modelId = "Unknown";
    const char* modelIdStr = device->GetModelID();
    if (modelIdStr && strlen(modelIdStr) > 0) {
        modelId = QString(modelIdStr);
    }
    m_deviceTable->setItem(row, 2, new QTableWidgetItem(modelId));
    
    // Mfg Serial Number - use virtual method
    QString serialNumber = "Unknown";
    const char* serialStr = device->GetModelSerialCode();
    if (serialStr && strlen(serialStr) > 0) {
        serialNumber = QString(serialStr);
    }
    m_deviceTable->setItem(row, 3, new QTableWidgetItem(serialNumber));
    
    // Device Instance
    uint8_t deviceInstance = device->GetDeviceInstance();
    QTableWidgetItem* instanceItem = new QTableWidgetItem(QString::number(deviceInstance));
    instanceItem->setTextAlignment(Qt::AlignCenter);
    m_deviceTable->setItem(row, 4, instanceItem);
    
    // Current Software - use virtual method
    QString softwareVersion = "-";
    const char* swCodeStr = device->GetSwCode();
    if (swCodeStr && strlen(swCodeStr) > 0) {
        softwareVersion = QString(swCodeStr);
    }
    m_deviceTable->setItem(row, 5, new QTableWidgetItem(softwareVersion));
    
    // Installation Description 1 and 2 - separate columns for PGN 126998 fields
    QString installDesc1 = "-";
    QString installDesc2 = "-";
    const char* installDesc1Ptr = device->GetInstallationDescription1();
    const char* installDesc2Ptr = device->GetInstallationDescription2();
    
    if (installDesc1Ptr && strlen(installDesc1Ptr) > 0) {
        installDesc1 = QString(installDesc1Ptr);
    }
    if (installDesc2Ptr && strlen(installDesc2Ptr) > 0) {
        installDesc2 = QString(installDesc2Ptr);
    }
    
    m_deviceTable->setItem(row, 6, new QTableWidgetItem(installDesc1));
    m_deviceTable->setItem(row, 7, new QTableWidgetItem(installDesc2));
    
    // Set text color based on activity status
    QColor textColor = isActive ? QColor(Qt::black) : QColor(Qt::gray);
    for (int col = 0; col < m_deviceTable->columnCount(); col++) {
        QTableWidgetItem* item = m_deviceTable->item(row, col);
        if (item) {
            item->setForeground(QBrush(textColor));
        }
    }
}

void DeviceMainWindow::displayLumitecMessage(const tN2kMsg& msg, const QString& description) {
    // Update status bar to show the latest Lumitec message
    QString statusText = QString("Lumitec Poco (Src:%1): %2")
                        .arg(msg.Source)
                        .arg(description);
    
    if (m_statusLabel) {
        m_statusLabel->setText(statusText);
    }
    
    // Also log to debug output
    qDebug() << "Lumitec Poco Message:" << statusText;
}

void DeviceMainWindow::updatePGNDialogDeviceList()
{
    // Build a list of current devices for the filter combo boxes
    QStringList devices;
    
    for (int row = 0; row < m_deviceTable->rowCount(); row++) {
        QTableWidgetItem* nodeItem = m_deviceTable->item(row, 0);        // Node address
        QTableWidgetItem* manufacturerItem = m_deviceTable->item(row, 1); // Manufacturer
        QTableWidgetItem* modelItem = m_deviceTable->item(row, 2);        // Model ID
        
        if (nodeItem && manufacturerItem) {
            QString nodeAddr = nodeItem->text();
            QString manufacturer = manufacturerItem->text();
            QString model = modelItem ? modelItem->text() : "";
            
            // Format: "Manufacturer Model (0xXX)" or "Manufacturer (0xXX)" if no model
            QString deviceName;
            if (!model.isEmpty() && model != "Unknown") {
                deviceName = QString("%1 %2").arg(manufacturer, model);
            } else {
                deviceName = manufacturer;
            }
            
            QString deviceEntry = QString("%1 (0x%2)")
                                .arg(deviceName)
                                .arg(nodeAddr);
            devices.append(deviceEntry);
        }
    }
    
    // Update all PGN dialogs' device lists
    for (PGNLogDialog* dialog : m_pgnLogDialogs) {
        if (dialog) {
            dialog->updateDeviceList(devices);
        }
    }
}

// Helper to send NMEA 2000 Command for PGN 130561 zone control via PGN 126208 Group Function
void DeviceMainWindow::sendZonePGN130561(uint8_t targetAddress, uint8_t zoneId, const QString& zoneName,
                                         uint8_t red, uint8_t green, uint8_t blue, uint16_t colorTemp,
                                         uint8_t intensity, uint8_t programId, uint8_t programColorSeqIndex,
                                         uint8_t programIntensity, uint8_t programRate,
                                         uint8_t programColorSequence, bool zoneEnabled) {
    if (!nmea2000) {
        qDebug() << "NMEA2000 not initialized, cannot send zone command";
        return;
    }

    tN2kMsg msg;
    msg.SetPGN(126208UL);  // Group Function PGN
    msg.Priority = 3;      // Standard priority for commands
    msg.Destination = targetAddress;
    
    // Group Function Header
    msg.AddByte(1);  // N2kgfc_Command - Command Group Function
    msg.Add3ByteInt(130561UL);  // Target PGN for zone lighting
    msg.AddByte(0x08);  // Priority Setting (standard value for commands)
    
    // Detect if this is a simple ON/OFF command (default parameters with only zone enable/disable change)
    bool isSimpleOnOff = (colorTemp == 3000 && programId == 0 && programColorSeqIndex == 0 && 
                          programIntensity == 0 && programRate == 0 && programColorSequence == 0 &&
                          ((red == 255 && green == 255 && blue == 255 && intensity == 200) ||  // ON case
                           (red == 0 && green == 0 && blue == 0 && intensity == 0)));           // OFF case
    
    if (isSimpleOnOff) {
        // For simple ON/OFF, send only Zone ID and Zone Enabled status
        uint8_t numberOfPairs = 2;
        msg.AddByte(numberOfPairs);
        
        // Field 1: Zone ID (0-252)
        msg.AddByte(1);  // Field number
        msg.AddByte(zoneId);
        
        // Field 13: Zone Enabled (using 2 LSb of last byte) + 6 reserved bits
        msg.AddByte(13);  // Field number
        uint8_t statusByte = zoneEnabled ? 0x01 : 0x00; // Set bit 0 for enabled/disabled
        msg.AddByte(statusByte);
        
        if (nmea2000->SendMsg(msg)) {
            // Blink TX indicator for transmitted messages
            blinkTxIndicator();
            
            // Log the sent message to all PGN log dialogs
            for (PGNLogDialog* dialog : m_pgnLogDialogs) {
                if (dialog && dialog->isVisible()) {
                    dialog->appendSentMessage(msg);
                }
            }
            qDebug() << "Sent Simple Zone Command (PGN 126208->130561) - Target:" << QString("0x%1").arg(targetAddress, 2, 16, QChar('0'))
                     << "Zone:" << zoneId << "Action:" << (zoneEnabled ? "ON" : "OFF");
        } else {
            qDebug() << "Failed to send simple zone command message";
        }
    } else {
        // For full control, send all parameters
        uint8_t numberOfPairs = 13;
        msg.AddByte(numberOfPairs);
        
        // Field/Value pairs for PGN 130561 fields
        // Each pair: field number (1 byte) + field value (variable length)
        
        // Field 1: Zone ID (0-252)
        msg.AddByte(1);  // Field number
        msg.AddByte(zoneId);
        
        // Field 2: Zone Name (variable length string)
        msg.AddByte(2);  // Field number
        msg.AddVarStr(zoneName.toLocal8Bit().constData(),false,32,16);
        
        // Field 3: Red Component (0-255)
        msg.AddByte(3);  // Field number
        msg.AddByte(red);
        
        // Field 4: Green Component (0-255)
        msg.AddByte(4);  // Field number
        msg.AddByte(green);
        
        // Field 5: Blue Component (0-255)
        msg.AddByte(5);  // Field number
        msg.AddByte(blue);
        
        // Field 6: Color Temperature (0-65532 Kelvin)
        msg.AddByte(6);  // Field number
        msg.Add2ByteUInt(colorTemp);
        
        // Field 7: Intensity/Brightness (0-200 * 0.5%)
        msg.AddByte(7);  // Field number
        msg.AddByte(intensity);
        
        // Field 8: Program ID (0-252)
        msg.AddByte(8);  // Field number
        msg.AddByte(programId);
        
        // Field 9: Program Color Sequence Index (0-252)
        msg.AddByte(9);  // Field number
        msg.AddByte(programColorSeqIndex);
        
        // Field 10: Program Intensity (0-200 * 0.5%)
        msg.AddByte(10);  // Field number
        msg.AddByte(programIntensity);
        
        // Field 11: Program Rate (0-200 * 0.5%)
        msg.AddByte(11);  // Field number
        msg.AddByte(programRate);
        
        // Field 12: Program Color Sequence (0-200 * 0.5%)
        msg.AddByte(12);  // Field number
        msg.AddByte(programColorSequence);
        
        // Field 13: Zone Enabled (using 2 LSb of last byte) + 6 reserved bits
        msg.AddByte(13);  // Field number
        uint8_t statusByte = zoneEnabled ? 0x01 : 0x00; // Set bit 0 for enabled/disabled
        msg.AddByte(statusByte);

        if (nmea2000->SendMsg(msg)) {
            // Blink TX indicator for transmitted messages
            blinkTxIndicator();
            
            // Log the sent message to all PGN log dialogs
            for (PGNLogDialog* dialog : m_pgnLogDialogs) {
                if (dialog && dialog->isVisible()) {
                    dialog->appendSentMessage(msg);
                }
            }
            qDebug() << "Sent Full Zone Command (PGN 126208->130561) - Target:" << QString("0x%1").arg(targetAddress, 2, 16, QChar('0'))
                     << "Zone:" << zoneId << "Name:" << zoneName 
                     << "RGB:" << red << green << blue << "Intensity:" << intensity;
        } else {
            qDebug() << "Failed to send full zone command message";
        }
    }
}

void DeviceMainWindow::onConnectClicked()
{
    if (!nmea2000) {
        // Reinitialize the NMEA2000 connection
        initNMEA2000();
        m_statusLabel->setText("NMEA2000 interface connected.");
    } else {
        m_statusLabel->setText("Already connected.");
    }
}

void DeviceMainWindow::onDisconnectClicked()
{
    if (nmea2000) {
        // Cleanup and delete the NMEA2000 connection
        delete nmea2000;
        nmea2000 = nullptr;
        
        // Cleanup device list
        if (m_deviceList) {
            delete m_deviceList;
            m_deviceList = nullptr;
        }
        
        // Clear the device table and activity tracking when disconnecting
        m_deviceTable->setRowCount(0);
        m_deviceActivity.clear();
        
        // Clear conflict history when disconnecting
        clearConflictHistory();
        
        m_isConnected = false;
        updateConnectionButtonStates();
        m_statusLabel->setText("NMEA2000 interface disconnected.");
    } else {
        m_statusLabel->setText("Already disconnected.");
    }
}

void DeviceMainWindow::updateConnectionButtonStates()
{
    if (m_connectButton && m_disconnectButton) {
        m_connectButton->setEnabled(!m_isConnected);
        m_disconnectButton->setEnabled(m_isConnected);
    }
}

void DeviceMainWindow::setupActivityIndicators()
{
    // Create TX indicator (red LED)
    m_txIndicator = new QLabel();
    m_txIndicator->setFixedSize(12, 12);
    m_txIndicator->setStyleSheet(
        "QLabel {"
        "    background-color: #400000;"  // Dark red when off
        "    border-radius: 6px;"
        "    border: 1px solid #333;"
        "}"
    );
    m_txIndicator->setToolTip("TX Activity (Red = Transmitting)");
    
    // Create RX indicator (green LED)
    m_rxIndicator = new QLabel();
    m_rxIndicator->setFixedSize(12, 12);
    m_rxIndicator->setStyleSheet(
        "QLabel {"
        "    background-color: #004000;"  // Dark green when off
        "    border-radius: 6px;"
        "    border: 1px solid #333;"
        "}"
    );
    m_rxIndicator->setToolTip("RX Activity (Green = Receiving)");
    
    // Create timers for blinking
    m_txBlinkTimer = new QTimer(this);
    m_txBlinkTimer->setSingleShot(true);
    connect(m_txBlinkTimer, &QTimer::timeout, this, &DeviceMainWindow::onTxBlinkTimeout);
    
    m_rxBlinkTimer = new QTimer(this);
    m_rxBlinkTimer->setSingleShot(true);
    connect(m_rxBlinkTimer, &QTimer::timeout, this, &DeviceMainWindow::onRxBlinkTimeout);
}

void DeviceMainWindow::blinkTxIndicator()
{
    // Turn on the red LED
    m_txIndicator->setStyleSheet(
        "QLabel {"
        "    background-color: #FF0000;"  // Bright red when active
        "    border-radius: 6px;"
        "    border: 1px solid #333;"
        "}"
    );
    
    // Start timer to turn it off after 50ms
    m_txBlinkTimer->start(50);
}

void DeviceMainWindow::blinkRxIndicator()
{
    // Turn on the green LED
    m_rxIndicator->setStyleSheet(
        "QLabel {"
        "    background-color: #00FF00;"  // Bright green when active
        "    border-radius: 6px;"
        "    border: 1px solid #333;"
        "}"
    );
    
    // Start timer to turn it off after 50ms
    m_rxBlinkTimer->start(50);
}

void DeviceMainWindow::onTxBlinkTimeout()
{
    // Turn off the red LED
    m_txIndicator->setStyleSheet(
        "QLabel {"
        "    background-color: #400000;"  // Dark red when off
        "    border-radius: 6px;"
        "    border: 1px solid #333;"
        "}"
    );
}

void DeviceMainWindow::onRxBlinkTimeout()
{
    // Turn off the green LED
    m_rxIndicator->setStyleSheet(
        "QLabel {"
        "    background-color: #004000;"  // Dark green when off
        "    border-radius: 6px;"
        "    border: 1px solid #333;"
        "}"
    );
}

void DeviceMainWindow::onPGNLogDialogDestroyed(QObject* obj)
{
    // Remove the destroyed dialog from our list
    PGNLogDialog* dialog = static_cast<PGNLogDialog*>(obj);
    m_pgnLogDialogs.removeAll(dialog);
    qDebug() << "PGN log dialog destroyed. Remaining dialogs: " << m_pgnLogDialogs.size();
}

void DeviceMainWindow::retryProductInformation(uint8_t targetAddress)
{
    // Check if we've reached max retries
    int currentRetries = m_productInfoRetryCount.value(targetAddress, 0);
    if (currentRetries >= MAX_PRODUCT_INFO_RETRIES) {
        qDebug() << "Max retries reached for Product Information from device"
                 << QString("0x%1").arg(targetAddress, 2, 16, QChar('0')).toUpper()
                 << "- giving up";
        
        // Clean up tracking
        m_pendingProductInfoRequests.remove(targetAddress);
        m_productInfoRetryCount.remove(targetAddress);
        cancelProductInfoRetry(targetAddress);
        return;
    }
    
    // Check if device is still pending (no response received yet)
    if (!m_pendingProductInfoRequests.contains(targetAddress)) {
        // Response was already received, cancel retry
        cancelProductInfoRetry(targetAddress);
        return;
    }
    
    // Increment retry count and resend request
    m_productInfoRetryCount[targetAddress] = currentRetries + 1;
    
    qDebug() << "Retrying Product Information request to device"
             << QString("0x%1").arg(targetAddress, 2, 16, QChar('0')).toUpper()
             << "(attempt" << (currentRetries + 2) << "of" << (MAX_PRODUCT_INFO_RETRIES + 1) << ")";
    
    // Send the request again
    if (nmea2000) {
        tN2kMsg N2kMsg;
        SetN2kPGN59904(N2kMsg, targetAddress, N2kPGNProductInformation);
        
        if (nmea2000->SendMsg(N2kMsg)) {
            blinkTxIndicator();
            
            // Start another retry timer
            QTimer* retryTimer = new QTimer();
            retryTimer->setSingleShot(true);
            connect(retryTimer, &QTimer::timeout, [this, targetAddress]() {
                retryProductInformation(targetAddress);
            });
            
            // Cancel old timer and set new one
            cancelProductInfoRetry(targetAddress);
            m_productInfoRetryTimers[targetAddress] = retryTimer;
            retryTimer->start(PRODUCT_INFO_RETRY_TIMEOUT_MS);
            
            // Log the sent message to all PGN log dialogs
            for (PGNLogDialog* dialog : m_pgnLogDialogs) {
                if (dialog && dialog->isVisible()) {
                    dialog->appendSentMessage(N2kMsg);
                }
            }
        }
    }
}

void DeviceMainWindow::cancelProductInfoRetry(uint8_t targetAddress)
{
    if (m_productInfoRetryTimers.contains(targetAddress)) {
        QTimer* timer = m_productInfoRetryTimers[targetAddress];
        if (timer) {
            timer->stop();
            timer->deleteLater();
        }
        m_productInfoRetryTimers.remove(targetAddress);
    }
}

uint8_t DeviceMainWindow::suggestAvailableInstance(unsigned long pgn, uint8_t excludeDeviceAddress) const
{
    if (!m_conflictAnalyzer) {
        return 0; // Default fallback
    }
    
    // Get all used instances for this PGN (excluding the device we're changing)
    QSet<uint8_t> usedInstances = m_conflictAnalyzer->getUsedInstancesForPGN(pgn, excludeDeviceAddress);
    
    // Find the first available instance starting from 0
    for (uint8_t instance = 0; instance <= 253; ++instance) {
        if (!usedInstances.contains(instance)) {
            return instance;
        }
    }
    
    // If all instances are somehow used (highly unlikely), return 253
    return 253;
}

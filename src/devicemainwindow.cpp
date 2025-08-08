#include "devicemainwindow.h"
#include "pgnlogdialog.h"
#include "pgndialog.h"
#include "NMEA2000_SocketCAN.h"
#include <QDir>
#include <QProcess>
#include <QMessageBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QWidget>
#include <QDebug>
#include <QRegExp>
#include <QToolBar>
#include <QDateTime>
#include <QApplication>
#include <QMenuBar>
#include <QStatusBar>

tNMEA2000_SocketCAN* nmea2000;
extern char can_interface[80];

// Static instance for message handling
DeviceMainWindow* DeviceMainWindow::instance = nullptr;

DeviceMainWindow::DeviceMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_centralWidget(nullptr)
    , m_deviceTable(nullptr)
    , m_refreshButton(nullptr)
    , m_analyzeButton(nullptr)
    , m_pgnLogButton(nullptr)
    , m_sendPGNButton(nullptr)
    , m_clearConflictsButton(nullptr)
    , m_statusLabel(nullptr)
    , m_updateTimer(nullptr)
    , m_canInterfaceCombo(nullptr)
    , m_deviceList(nullptr)
    , m_pgnLogDialog(nullptr)
{
    DeviceMainWindow::instance = this;  // Capture 'this' for static callback
    
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
    
    setWindowTitle("NMEA2000 Network Analyzer");
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
    
    // Top toolbar with CAN interface selector
    QHBoxLayout* toolbarLayout = new QHBoxLayout();
    
    QLabel* canLabel = new QLabel("CAN Interface:");
    m_canInterfaceCombo = new QComboBox();
    m_canInterfaceCombo->setMinimumWidth(120);
    connect(m_canInterfaceCombo, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
            this, &DeviceMainWindow::onCanInterfaceChanged);
    
    toolbarLayout->addWidget(canLabel);
    toolbarLayout->addWidget(m_canInterfaceCombo);
    toolbarLayout->addStretch();
    
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
            << "Instance" << "Type" << "Current Software" << "Installation Description";
    m_deviceTable->setHorizontalHeaderLabels(headers);
    
    // Configure table
    m_deviceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_deviceTable->setAlternatingRowColors(true);
    m_deviceTable->horizontalHeader()->setStretchLastSection(true);
    m_deviceTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_deviceTable->verticalHeader()->setVisible(false);
    m_deviceTable->setSortingEnabled(true);
    
    // Set smaller font for the table
    QFont tableFont = m_deviceTable->font();
    tableFont.setPointSize(9);
    m_deviceTable->setFont(tableFont);
    
    // Connect row selection signal for conflict highlighting
    connect(m_deviceTable->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &DeviceMainWindow::onRowSelectionChanged);
    
    mainLayout->addWidget(m_deviceTable);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    m_refreshButton = new QPushButton("Refresh Devices");
    m_analyzeButton = new QPushButton("Analyze Instance Conflicts");
    m_pgnLogButton = new QPushButton("Show PGN Log");
    m_sendPGNButton = new QPushButton("Send PGN");
    m_clearConflictsButton = new QPushButton("Clear Conflict History");
    
    connect(m_refreshButton, &QPushButton::clicked, this, &DeviceMainWindow::onRefreshClicked);
    connect(m_analyzeButton, &QPushButton::clicked, this, &DeviceMainWindow::analyzeInstanceConflicts);
    connect(m_pgnLogButton, &QPushButton::clicked, this, &DeviceMainWindow::showPGNLog);
    connect(m_sendPGNButton, &QPushButton::clicked, this, &DeviceMainWindow::showSendPGNDialog);
    connect(m_clearConflictsButton, &QPushButton::clicked, this, &DeviceMainWindow::clearConflictHistory);
    
    buttonLayout->addWidget(m_refreshButton);
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
    toolsMenu->addAction("&Analyze Instance Conflicts", this, &DeviceMainWindow::analyzeInstanceConflicts);
    toolsMenu->addAction("&Clear Conflict History", this, &DeviceMainWindow::clearConflictHistory);
    
    // Help menu
    QMenu* helpMenu = menuBar->addMenu("&Help");
    helpMenu->addAction("&About", this, []() {
        QMessageBox::about(nullptr, "About NMEA2000 Network Analyzer\n", 
                          "NMEA2000 Network Analyzer v1.0\n\n"
                          "Professional NMEA2000 network diagnostic tool with\n"
                          "real-time PGN instance conflict detection.\n\n"
                          "Built with Qt and NMEA2000 library.");
    });
}

void DeviceMainWindow::initNMEA2000()
{
    qDebug() << "initNMEA2000() called with interface:" << m_currentInterface;
    
    if (nmea2000 != nullptr) {
        delete nmea2000;
        nmea2000 = nullptr;
    }

    // Create and initialize the NMEA2000 interface FIRST
    qDebug() << "Creating tNMEA2000_SocketCAN with interface:" << m_currentInterface;
    qDebug() << "Global can_interface variable:" << can_interface;
    nmea2000 = new tNMEA2000_SocketCAN(can_interface);
    
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
    // Track PGN instances for conflict detection
    trackPGNInstance(msg);

    // Forward to PGN log dialog if it exists and is visible
    if (m_pgnLogDialog && m_pgnLogDialog->isVisible()) {
        m_pgnLogDialog->appendMessage(msg);
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
    
    // If no CAN interfaces found, add some common ones for testing
    if (interfaces.isEmpty()) {
        interfaces << "can0" << "can1" << "vcan0" << "vcan1";
    }
    
    return interfaces;
}

void DeviceMainWindow::onCanInterfaceChanged(const QString &interface)
{
    if (interface != m_currentInterface && !interface.isEmpty()) {
        m_currentInterface = interface;
        strncpy(can_interface, interface.toLocal8Bit().data(), sizeof(can_interface) - 1);
        can_interface[sizeof(can_interface) - 1] = '\0';
        
        reinitializeNMEA2000();
    }
}

void DeviceMainWindow::reinitializeNMEA2000()
{
    if (nmea2000 != nullptr) {
        delete nmea2000;
        nmea2000 = nullptr;
    }
    
    // Clear conflict history when changing interface
    clearConflictHistory();
    
    initNMEA2000();
    updateDeviceList();
}

// Include all the device table population and conflict detection methods from devicelistdialog.cpp
void DeviceMainWindow::updateDeviceList()
{
    if (!nmea2000) {
        m_statusLabel->setText("NMEA2000 interface not initialized");
        m_statusLabel->setStyleSheet("font-weight: bold; color: red; padding: 5px; background-color: #ffe6e6; border: 1px solid #ff9999; border-radius: 3px;");
        return;
    }
    
    populateDeviceTable();
}

void DeviceMainWindow::populateDeviceTable()
{
    // Clear existing rows
    m_deviceTable->setRowCount(0);
    
    if (!m_deviceList) {
        m_statusLabel->setText("Device list not available");
        m_statusLabel->setStyleSheet("font-weight: bold; color: red; padding: 5px; background-color: #ffe6e6; border: 1px solid #ff9999; border-radius: 3px;");
        return;
    }
    
    int deviceCount = 0;
    
    // Iterate through all possible source addresses (0-253)
    for (uint8_t source = 0; source < N2kMaxBusDevices; source++) {
        const tNMEA2000::tDevice* device = m_deviceList->FindDeviceBySource(source);
        if (device) {
            m_deviceTable->insertRow(deviceCount);
            
            // Node Address (Source) - in hex format like Maretron
            QTableWidgetItem* nodeAddressItem = new QTableWidgetItem(QString("%1").arg(source, 2, 16, QChar('0')).toUpper());
            nodeAddressItem->setTextAlignment(Qt::AlignCenter);
            m_deviceTable->setItem(deviceCount, 0, nodeAddressItem);
            
            // Manufacturer - convert manufacturer code to name
            uint16_t manufacturerCode = device->GetManufacturerCode();
            QString manufacturerName = getManufacturerName(manufacturerCode);
            m_deviceTable->setItem(deviceCount, 1, new QTableWidgetItem(manufacturerName));
            
            // Mfg Model ID - use virtual method
            QString modelId = "Unknown";
            const char* modelIdStr = device->GetModelID();
            if (modelIdStr && strlen(modelIdStr) > 0) {
                modelId = QString(modelIdStr);
            }
            m_deviceTable->setItem(deviceCount, 2, new QTableWidgetItem(modelId));
            
            // Mfg Serial Number - use virtual method
            QString serialNumber = "Unknown";
            const char* serialStr = device->GetModelSerialCode();
            if (serialStr && strlen(serialStr) > 0) {
                serialNumber = QString(serialStr);
            }
            m_deviceTable->setItem(deviceCount, 3, new QTableWidgetItem(serialNumber));
            
            // Device Instance
            uint8_t deviceInstance = device->GetDeviceInstance();
            QTableWidgetItem* instanceItem = new QTableWidgetItem(QString::number(deviceInstance));
            instanceItem->setTextAlignment(Qt::AlignCenter);
            m_deviceTable->setItem(deviceCount, 4, instanceItem);
            
            // Type - create based on device function and class
            QString type = getDeviceFunctionName(device->GetDeviceFunction());
            if (type.startsWith("Unknown")) {
                type = getDeviceClassName(device->GetDeviceClass());
            }
            if (type.startsWith("Unknown")) {
                type = QString("Device %1").arg(source, 2, 16, QChar('0')).toUpper();
            }
            m_deviceTable->setItem(deviceCount, 5, new QTableWidgetItem(type));
            
            // Current Software - use virtual method
            QString softwareVersion = "-";
            const char* swCodeStr = device->GetSwCode();
            if (swCodeStr && strlen(swCodeStr) > 0) {
                softwareVersion = QString(swCodeStr);
            }
            m_deviceTable->setItem(deviceCount, 6, new QTableWidgetItem(softwareVersion));
            
            // Installation Description - combine InstallationDescription1 and InstallationDescription2 from PGN 126998
            QString installDesc = "";
            const char* installDesc1 = device->GetInstallationDescription1();
            const char* installDesc2 = device->GetInstallationDescription2();
            
            if (installDesc1 && strlen(installDesc1) > 0) {
                installDesc = QString(installDesc1);
            }
            if (installDesc2 && strlen(installDesc2) > 0) {
                if (!installDesc.isEmpty()) {
                    installDesc += " / ";
                }
                installDesc += QString(installDesc2);
            }
            if (installDesc.isEmpty()) {
                installDesc = "-";
            }
            m_deviceTable->setItem(deviceCount, 7, new QTableWidgetItem(installDesc));
            
            deviceCount++;
        }
    }
    
    // Update status
    if (deviceCount == 0) {
        m_statusLabel->setText("No NMEA2000 devices detected on the network");
        m_statusLabel->setStyleSheet("font-weight: bold; color: orange; padding: 5px; background-color: #fff3cd; border: 1px solid #ffeaa7; border-radius: 3px;");
    } else {
        QString statusText = QString("Found %1 NMEA2000 device(s) - Auto-updating every 2 seconds").arg(deviceCount);
        QString statusStyle = "font-weight: bold; color: green; padding: 5px; background-color: #e6ffe6; border: 1px solid #99ff99; border-radius: 3px;";
        
        // Check for instance conflicts and update status accordingly
        if (!m_instanceConflicts.isEmpty()) {
            statusText += QString(" - WARNING: %1 instance conflict(s) detected!").arg(m_instanceConflicts.size());
            statusStyle = "font-weight: bold; color: red; padding: 5px; background-color: #ffe6e6; border: 1px solid #ff9999; border-radius: 3px;";
        }
        
        m_statusLabel->setText(statusText);
        m_statusLabel->setStyleSheet(statusStyle);
    }
    
    // Analyze and highlight instance conflicts
    highlightInstanceConflicts();
    
    // Resize columns to content
    m_deviceTable->resizeColumnsToContents();
}

// Slot implementations
void DeviceMainWindow::onRefreshClicked()
{
    updateDeviceList();
}

void DeviceMainWindow::onRowSelectionChanged()
{
    // Implementation similar to DeviceListDialog::onRowSelectionChanged()
    // Will add later - for now just a stub
}

void DeviceMainWindow::showPGNLog()
{
    if (!m_pgnLogDialog) {
        m_pgnLogDialog = new PGNLogDialog(this);
    }
    m_pgnLogDialog->show();
    m_pgnLogDialog->raise();
    m_pgnLogDialog->activateWindow();
}

void DeviceMainWindow::showSendPGNDialog()
{
    PGNDialog* pgnDialog = new PGNDialog(this);
    pgnDialog->exec();
    delete pgnDialog;
}

void DeviceMainWindow::clearConflictHistory()
{
    m_pgnInstances.clear();
    m_instanceConflicts.clear();
    m_conflictingSources.clear();
    updateDeviceList(); // Refresh display
}

// PGN instance tracking methods (from MainWindow)
void DeviceMainWindow::trackPGNInstance(const tN2kMsg& msg) {
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

uint8_t DeviceMainWindow::extractInstanceFromPGN(const tN2kMsg& msg) {
    // Extract instance data based on PGN type
    if (msg.DataLen < 1) {
        return 255; // Invalid
    }
    
    uint8_t instance = 255;
    
    switch (msg.PGN) {
        case 127488: // Engine Parameters, Rapid
        case 127489: // Engine Parameters, Dynamic
        case 127502: // Binary Switch Bank Control
        case 127505: // Fluid Level
        case 127508: // Battery Status
        case 127509: // Inverter Status
        case 127513: // Battery Configuration Status
            if (msg.DataLen >= 1) {
                instance = msg.Data[0]; // Instance is first byte
            }
            break;
            
        case 130312: // Temperature
        case 130314: // Actual Pressure
        case 130316: // Temperature, Extended Range
            if (msg.DataLen >= 2) {
                instance = msg.Data[1]; // Instance is second byte (after SID)
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

bool DeviceMainWindow::isPGNWithInstance(unsigned long pgn) {
    // List of PGNs that contain instance data
    static QSet<unsigned long> instancePGNs = {
        127488, // Engine Parameters, Rapid
        127489, // Engine Parameters, Dynamic
        127502, // Binary Switch Bank Control
        127505, // Fluid Level
        127508, // Battery Status
        127509, // Inverter Status
        127513, // Battery Configuration Status
        130312, // Temperature
        130314, // Actual Pressure
        130316, // Temperature, Extended Range
    };
    
    return instancePGNs.contains(pgn);
}

void DeviceMainWindow::updateInstanceConflicts() {
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

// Getter methods for conflict data
QList<InstanceConflict> DeviceMainWindow::getInstanceConflicts() const {
    return m_instanceConflicts;
}

bool DeviceMainWindow::hasInstanceConflicts() const {
    return !m_instanceConflicts.isEmpty();
}

QSet<uint8_t> DeviceMainWindow::getConflictingSources() const {
    return m_conflictingSources;
}

// Stub implementations for now - will copy from DeviceListDialog later
void DeviceMainWindow::highlightInstanceConflicts() {
    // Reset all row colors first
    for (int row = 0; row < m_deviceTable->rowCount(); row++) {
        for (int col = 0; col < m_deviceTable->columnCount(); col++) {
            QTableWidgetItem* item = m_deviceTable->item(row, col);
            if (item) {
                item->setBackground(QBrush()); // Reset to default background
            }
        }
    }
    
    // Highlight conflicting devices in red
    if (!m_conflictingSources.isEmpty()) {
        for (int row = 0; row < m_deviceTable->rowCount(); row++) {
            QTableWidgetItem* nodeAddressItem = m_deviceTable->item(row, 0); // Node Address column
            if (nodeAddressItem) {
                // Convert hex node address back to uint8_t for comparison
                bool ok;
                uint8_t source = nodeAddressItem->text().toUInt(&ok, 16);
                
                if (ok && m_conflictingSources.contains(source)) {
                    // Highlight this entire row in red
                    for (int col = 0; col < m_deviceTable->columnCount(); col++) {
                        QTableWidgetItem* item = m_deviceTable->item(row, col);
                        if (item) {
                            item->setBackground(QBrush(QColor(255, 200, 200))); // Light red background
                            item->setForeground(QBrush(QColor(139, 0, 0))); // Dark red text
                        }
                    }
                }
            }
        }
    }
}

void DeviceMainWindow::analyzeInstanceConflicts() {
    if (m_instanceConflicts.isEmpty()) {
        QMessageBox::information(this, "Instance Conflict Analysis", 
                                "No instance conflicts detected.\n\n"
                                "All devices are using unique instance numbers for their PGN types.");
        return;
    }
    
    QString analysisText = QString("Found %1 instance conflict(s):\n\n").arg(m_instanceConflicts.size());
    
    for (const InstanceConflict& conflict : m_instanceConflicts) {
        analysisText += QString("PGN %1 (%2), Instance %3:\n")
                       .arg(conflict.pgn)
                       .arg(getPGNName(conflict.pgn))
                       .arg(conflict.instance);
        
        analysisText += "  Conflicting sources: ";
        QStringList sourceList;
        for (uint8_t source : conflict.conflictingSources) {
            sourceList.append(QString("0x%1").arg(source, 2, 16, QChar('0')).toUpper());
        }
        analysisText += sourceList.join(", ") + "\n\n";
    }
    
    analysisText += "Recommendation: Check device configuration to ensure each device type uses unique instance numbers.";
    
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Instance Conflict Analysis");
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setText("Instance conflicts detected on the NMEA2000 network!");
    msgBox.setDetailedText(analysisText);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
}

QString DeviceMainWindow::getDeviceClassName(unsigned char deviceClass) {
    return QString("Device Class %1").arg(deviceClass);
}

QString DeviceMainWindow::getDeviceFunctionName(unsigned char deviceFunction) {
    return QString("Function %1").arg(deviceFunction);
}

QString DeviceMainWindow::getManufacturerName(uint16_t manufacturerCode) {
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
        case 358:  return "Victron";
        case 504:  return "Vesper";
        case 1403: return "Arco";
        case 1512: return "Lumitec";
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
        default: return QString("PGN %1").arg(pgn);
    }
}

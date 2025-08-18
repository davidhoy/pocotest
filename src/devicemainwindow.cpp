#include "devicemainwindow.h"
#include <cstdint>
#include "pgnlogdialog.h"
#include "pgndialog.h"
#include "pocodevicedialog.h"
#include "zonelightingdialog.h"
#include "LumitecPoco.h"
#include "NMEA2000_SocketCAN.h"
#include <NMEA2000.h>
#include <QDir>
#include <QProcess>
#include <QMessageBox>
#include <QLabel>
#include <QDebug>
#include <QHBoxLayout>
#include <QWidget>
#include <QDebug>
#include <QRegExp>
#include <QToolBar>
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
#include <QDateTime>

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
    m_deviceTable->setColumnCount(9);
    
    QStringList headers;
    headers << "Node Address" << "Manufacturer" << "Model ID" << "Serial Number" 
            << "Instance" << "Type" << "Current Software" << "Installation 1" << "Installation 2";
    m_deviceTable->setHorizontalHeaderLabels(headers);
    
    // Configure table
    m_deviceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_deviceTable->setAlternatingRowColors(true);
    m_deviceTable->horizontalHeader()->setStretchLastSection(true);
    m_deviceTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_deviceTable->verticalHeader()->setVisible(false);
    m_deviceTable->setSortingEnabled(false); // Disable sorting to maintain device order
    
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
    // Update device activity tracking
    updateDeviceActivity(msg.Source);
    
    // Track PGN instances for conflict detection
    trackPGNInstance(msg);

    // Handle Lumitec Poco specific messages
    if (msg.PGN == LUMITEC_PGN_61184) {
        handleLumitecPocoMessage(msg);
    }
    
    // Handle Product Information responses (PGN 126996)
    if (msg.PGN == N2kPGNProductInformation) {
        handleProductInformationResponse(msg);
    }
    
    // Handle Group Function messages (PGN 126208) - including acknowledgments
    if (msg.PGN == 126208UL) {
        handleGroupFunctionMessage(msg);
    }

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
            } else {
                // New device - add to end
                int newRow = m_deviceTable->rowCount();
                m_deviceTable->insertRow(newRow);
                updateDeviceTableRow(newRow, source, device, isActive);
                m_deviceActivity[source].tableRow = newRow;
                deviceCount++;
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
    
    // Update PGN dialog device list if it exists
    updatePGNDialogDeviceList();
}

// Slot implementations
void DeviceMainWindow::onRefreshClicked()
{
    // Clear the device table and activity tracking
    m_deviceTable->setRowCount(0);
    m_deviceActivity.clear();
    
    // Force update the device list
    updateDeviceList();
}

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
    QTableWidgetItem* typeItem = m_deviceTable->item(row, 5);         // Type column
    
    if (!nodeAddressItem) {
        return;
    }
    
    QString nodeAddress = nodeAddressItem->text();
    QString manufacturer = manufacturerItem ? manufacturerItem->text() : "Unknown";
    QString model = modelItem ? modelItem->text() : "Unknown";
    QString type = typeItem ? typeItem->text() : "Unknown";
    
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
    
    // Query Device Configuration action
    QAction* queryConfigAction = contextMenu->addAction("Query Device Configuration");
    connect(queryConfigAction, &QAction::triggered, [this, sourceAddress]() {
        queryDeviceConfiguration(sourceAddress);
    });
    
    // Request Product Information action
    QAction* productInfoAction = contextMenu->addAction("Request Product Information");
    connect(productInfoAction, &QAction::triggered, [this, sourceAddress]() {
        requestProductInformation(sourceAddress);
    });
    
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
    qDebug() << "showPGNLog: Starting...";
    
    if (!m_pgnLogDialog) {
        qDebug() << "showPGNLog: Creating new PGNLogDialog...";
        m_pgnLogDialog = new PGNLogDialog(this);
        qDebug() << "showPGNLog: PGNLogDialog created successfully";
    }
    
    qDebug() << "showPGNLog: About to clear all filters...";
    // Clear all filters for general PGN log view
    m_pgnLogDialog->clearAllFilters();
    qDebug() << "showPGNLog: Filters cleared";
    
    qDebug() << "showPGNLog: About to show dialog...";
    m_pgnLogDialog->show();
    qDebug() << "showPGNLog: Dialog shown";
    
    qDebug() << "showPGNLog: About to raise dialog...";
    m_pgnLogDialog->raise();
    qDebug() << "showPGNLog: Dialog raised";
    
    qDebug() << "showPGNLog: About to activate window...";
    m_pgnLogDialog->activateWindow();
    qDebug() << "showPGNLog: Window activated - completed successfully";
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
    QString type = m_deviceTable->item(row, 5) ? m_deviceTable->item(row, 5)->text() : "Unknown";
    QString software = m_deviceTable->item(row, 6) ? m_deviceTable->item(row, 6)->text() : "Unknown";
    QString installDesc1 = m_deviceTable->item(row, 7) ? m_deviceTable->item(row, 7)->text() : "Unknown";
    QString installDesc2 = m_deviceTable->item(row, 8) ? m_deviceTable->item(row, 8)->text() : "Unknown";
    
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
            if (m_conflictingSources.contains(source)) {
                additionalInfo += "\n⚠️  WARNING: This device has instance conflicts!\n";
                
                // List the conflicting PGNs
                for (const InstanceConflict& conflict : m_instanceConflicts) {
                    if (conflict.conflictingSources.contains(source)) {
                        additionalInfo += QString("• PGN %1 (%2), Instance %3\n")
                                         .arg(conflict.pgn)
                                         .arg(getPGNName(conflict.pgn))
                                         .arg(conflict.instance);
                    }
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
        "Type: %6\n"
        "Software Version: %7\n"
        "Installation 1: %8\n"
        "Installation 2: %9\n\n"
        "Additional Info:\n%10"
    ).arg(nodeAddress, manufacturer, modelId, serialNumber, instance, type, software, installDesc1, installDesc2, additionalInfo);
    
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(QString("Device Details - 0x%1").arg(nodeAddress));
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setText(detailsText);  // Show all details directly in the main text area
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
}

void DeviceMainWindow::queryDeviceConfiguration(uint8_t targetAddress)
{
    // Send PGN 126996 (Product Information) request to the device
    if (!nmea2000) {
        QMessageBox::warning(this, "Error", "NMEA2000 interface not available");
        return;
    }
    
    // TODO: Implement sending of configuration query PGNs
    // This would typically involve sending PGN 59904 (ISO Request) 
    // to request specific PGNs like 126996, 126998, etc.
    
    QMessageBox::information(this, "Query Configuration", 
                            QString("Sending configuration query to device 0x%1...\n\n"
                                   "This feature will send standard NMEA2000 requests for:\n"
                                   "• Product Information (PGN 126996)\n"
                                   "• Configuration Information (PGN 126998)\n"
                                   "• Supported PGNs (PGN 126464)\n\n"
                                   "Implementation coming soon!")
                            .arg(targetAddress, 2, 16, QChar('0')).toUpper());
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
        // Track that we've requested product info from this device
        m_pendingProductInfoRequests.insert(targetAddress);
        
        // Log the sent message to PGN log if it's visible
        if (m_pgnLogDialog && m_pgnLogDialog->isVisible()) {
            m_pgnLogDialog->appendSentMessage(N2kMsg);
        }
        
        QMessageBox::information(this, "Request Sent", 
                                QString("Product information request sent to device 0x%1\n\n"
                                       "Sent ISO Request (PGN 59904) requesting:\n"
                                       "• Product Information (PGN 126996)\n\n"
                                       "The device should respond with its product details.\n"
                                       "A dialog will show the response when received.")
                                .arg(targetAddress, 2, 16, QChar('0')).toUpper());
    } else {
        QMessageBox::warning(this, "Send Failed", 
                            QString("Failed to send product information request to device 0x%1")
                            .arg(targetAddress, 2, 16, QChar('0')).toUpper());
    }
}

void DeviceMainWindow::showPGNLogForDevice(uint8_t sourceAddress)
{
    // Show PGN log dialog filtered for this specific device
    if (!m_pgnLogDialog) {
        m_pgnLogDialog = new PGNLogDialog(this);
    }
    
    // Set filters for both source and destination addresses to capture all traffic
    // involving this device (messages from it OR messages to it)
    m_pgnLogDialog->setSourceFilter(sourceAddress);
    m_pgnLogDialog->setDestinationFilter(sourceAddress);
    m_pgnLogDialog->setFilterLogic(true); // Use OR logic - show messages FROM device OR TO device
    
    m_pgnLogDialog->setWindowTitle(QString("PGN History - Device 0x%1 (Source OR Destination)")
                                   .arg(sourceAddress, 2, 16, QChar('0')).toUpper());
    
    m_pgnLogDialog->show();
    m_pgnLogDialog->raise();
    m_pgnLogDialog->activateWindow();
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
        case 61184: return "Lumitec Poco Proprietary";
        default: return QString("PGN %1").arg(pgn);
    }
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
    }
    
    // Basic parsing of Product Information (PGN 126996)
    if (msg.DataLen < 10) { // Minimum expected length
        qDebug() << "Product Information message too short: " << msg.DataLen << " bytes";
        return;
    }
    
    int Index = 0;
    uint16_t N2kVersion = msg.Get2ByteUInt(Index);
    uint16_t ProductCode = msg.Get2ByteUInt(Index);
    char ModelID[33] = {0}; // Max 32 chars + null terminator
    char SoftwareVersion[33] = {0};
    char ModelVersion[33] = {0};
    char SerialCode[33] = {0};
    
    // Read strings with length prefixes
    int len;
    
    // Model ID
    len = msg.GetByte(Index);
    if (len > 32) len = 32; // Prevent buffer overflow
    for (int i = 0; i < len && Index < msg.DataLen; i++) {
        ModelID[i] = msg.GetByte(Index);
    }
    
    // Software Version
    if (Index < msg.DataLen) {
        len = msg.GetByte(Index);
        if (len > 32) len = 32;
        for (int i = 0; i < len && Index < msg.DataLen; i++) {
            SoftwareVersion[i] = msg.GetByte(Index);
        }
    }
    
    // Model Version
    if (Index < msg.DataLen) {
        len = msg.GetByte(Index);
        if (len > 32) len = 32;
        for (int i = 0; i < len && Index < msg.DataLen; i++) {
            ModelVersion[i] = msg.GetByte(Index);
        }
    }
    
    // Serial Code
    if (Index < msg.DataLen) {
        len = msg.GetByte(Index);
        if (len > 32) len = 32;
        for (int i = 0; i < len && Index < msg.DataLen; i++) {
            SerialCode[i] = msg.GetByte(Index);
        }
    }
    
    if (showDialog) {
        // Show detailed dialog for responses to our explicit requests
        QString productInfo = QString(
            "Product Information Response from Device 0x%1:\n\n"
            "• N2K Version: %2\n"
            "• Product Code: %3\n"
            "• Model ID: %4\n"
            "• Software Version: %5\n"
            "• Model Version: %6\n"
            "• Serial Code: %7"
        ).arg(msg.Source, 2, 16, QChar('0'))
         .arg(N2kVersion)
         .arg(ProductCode)
         .arg(QString::fromLatin1(ModelID))
         .arg(QString::fromLatin1(SoftwareVersion))
         .arg(QString::fromLatin1(ModelVersion))
         .arg(QString::fromLatin1(SerialCode));
        
        QMessageBox::information(this, "Product Information Response", productInfo);
    }
    // Note: Unsolicited product information is silently ignored
}

// Group Function Message Handling (PGN 126208)
void DeviceMainWindow::handleGroupFunctionMessage(const tN2kMsg& msg) {
    if (msg.PGN != 126208UL || msg.DataLen < 5) {
        return;
    }
    
    int Index = 0;
    uint8_t groupFunction = msg.GetByte(Index);
    uint32_t targetPGN = msg.Get3ByteUInt(Index);
    
    // Check if this is an acknowledgment (Group Function code 2)
    if (groupFunction == 2) { // N2kgfc_Acknowledge
        msg.GetByte(Index); // Skip priority setting byte
        uint8_t numberOfParameters = msg.GetByte(Index);
        
        // Parse acknowledgment parameters
        bool success = true;
        if (numberOfParameters > 0) {
            // Read the first parameter to check for errors
            if (Index < msg.DataLen) {
                msg.GetByte(Index); // Skip parameter index
                uint8_t errorCode = msg.GetByte(Index);
                
                // Error codes: 0=ACK, 1=NAK, 2=Access Denied, etc.
                (void)errorCode;
                //success = (errorCode == 0);
            }
        }
        
        qDebug() << "Received Group Function ACK from device" << QString("0x%1").arg(msg.Source, 2, 16, QChar('0'))
                 << "for PGN" << targetPGN << "- Success:" << success;
        
        // Emit signal to notify waiting dialogs
        emit commandAcknowledged(msg.Source, targetPGN, success);
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
            // Log the sent message to PGN log if it's visible
            if (m_pgnLogDialog && m_pgnLogDialog->isVisible()) {
                m_pgnLogDialog->appendSentMessage(msg);
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
            // Log the sent message to PGN log if it's visible
            if (m_pgnLogDialog && m_pgnLogDialog->isVisible()) {
                m_pgnLogDialog->appendSentMessage(msg);
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
    
    for (auto it = m_deviceActivity.begin(); it != m_deviceActivity.end(); ++it) {
        qint64 msSinceLastSeen = it->lastSeen.msecsTo(now);
        if (msSinceLastSeen > DEVICE_TIMEOUT_MS) {
            it->isActive = false;
        }
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
    
    // Type - create based on device function and class
    QString type = getDeviceFunctionName(device->GetDeviceFunction());
    if (type.startsWith("Unknown")) {
        type = getDeviceClassName(device->GetDeviceClass());
    }
    if (type.startsWith("Unknown")) {
        type = QString("Device %1").arg(source, 2, 16, QChar('0')).toUpper();
    }
    m_deviceTable->setItem(row, 5, new QTableWidgetItem(type));
    
    // Current Software - use virtual method
    QString softwareVersion = "-";
    const char* swCodeStr = device->GetSwCode();
    if (swCodeStr && strlen(swCodeStr) > 0) {
        softwareVersion = QString(swCodeStr);
    }
    m_deviceTable->setItem(row, 6, new QTableWidgetItem(softwareVersion));
    
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
    
    m_deviceTable->setItem(row, 7, new QTableWidgetItem(installDesc1));
    m_deviceTable->setItem(row, 8, new QTableWidgetItem(installDesc2));
    
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
    // Only update if the PGN dialog exists
    if (!m_pgnLogDialog) {
        return;
    }
    
    // Build a list of current devices for the filter combo boxes
    QStringList devices;
    
    for (int row = 0; row < m_deviceTable->rowCount(); row++) {
        QTableWidgetItem* nodeItem = m_deviceTable->item(row, 0);  // Node address
        QTableWidgetItem* nameItem = m_deviceTable->item(row, 1);  // Device name
        
        if (nodeItem && nameItem) {
            QString nodeAddr = nodeItem->text();
            QString deviceName = nameItem->text();
            
            // Format: "Device Name (0xXX)"
            QString deviceEntry = QString("%1 (0x%2)")
                                .arg(deviceName)
                                .arg(nodeAddr);
            devices.append(deviceEntry);
        }
    }
    
    // Update the PGN dialog's device list
    m_pgnLogDialog->updateDeviceList(devices);
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
            if (m_pgnLogDialog && m_pgnLogDialog->isVisible()) {
                m_pgnLogDialog->appendSentMessage(msg);
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
        msg.AddVarStr(zoneName.toLocal8Bit().constData());
        
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
            if (m_pgnLogDialog && m_pgnLogDialog->isVisible()) {
                m_pgnLogDialog->appendSentMessage(msg);
            }
            qDebug() << "Sent Full Zone Command (PGN 126208->130561) - Target:" << QString("0x%1").arg(targetAddress, 2, 16, QChar('0'))
                     << "Zone:" << zoneId << "Name:" << zoneName 
                     << "RGB:" << red << green << blue << "Intensity:" << intensity;
        } else {
            qDebug() << "Failed to send full zone command message";
        }
    }
}

#include "devicelistdialog.h"
#include "mainwindow.h"
#include "NMEA2000_SocketCAN.h"
#include "N2kDeviceList.h"
#include <QMessageBox>
#include <QApplication>

extern tNMEA2000_SocketCAN* nmea2000;

DeviceListDialog::DeviceListDialog(QWidget *parent, tN2kDeviceList* deviceList)
    : QDialog(parent)
    , m_deviceTable(nullptr)
    , m_refreshButton(nullptr)
    , m_closeButton(nullptr)
    , m_statusLabel(nullptr)
    , m_updateTimer(nullptr)
    , m_deviceList(deviceList)
    , m_mainWindow(qobject_cast<MainWindow*>(parent))
{
    setupUI();
    
    // Set up auto-refresh timer (update every 2 seconds)
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &DeviceListDialog::updateDeviceList);
    m_updateTimer->start(2000);
    
    // Initial population
    updateDeviceList();
    
    setWindowTitle("NMEA2000 Device List");
    setModal(false);
    resize(800, 500);
}

DeviceListDialog::~DeviceListDialog()
{
    if (m_updateTimer) {
        m_updateTimer->stop();
    }
}

void DeviceListDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Status label
    m_statusLabel = new QLabel("Scanning for NMEA2000 devices...");
    m_statusLabel->setStyleSheet("font-weight: bold; color: #333;");
    m_statusLabel->setWordWrap(true);
    mainLayout->addWidget(m_statusLabel);
    
    // Device table
    m_deviceTable = new QTableWidget();
    m_deviceTable->setColumnCount(8);
    
    QStringList headers;
    headers << "Node Address" << "Manufacturer" << "Mfg Model ID" << "Mfg Serial Number" 
            << "Device Instance" << "Type" << "Current Software" << "Installation Description";
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
    tableFont.setPointSize(9);  // Smaller font size
    m_deviceTable->setFont(tableFont);
    
    // Connect row selection signal for conflict highlighting
    connect(m_deviceTable->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &DeviceListDialog::onRowSelectionChanged);
    
    mainLayout->addWidget(m_deviceTable);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    m_refreshButton = new QPushButton("Refresh Now");
    QPushButton* analyzeButton = new QPushButton("Analyze Instance Conflicts");
    m_closeButton = new QPushButton("Close");
    
    connect(m_refreshButton, &QPushButton::clicked, this, &DeviceListDialog::onRefreshClicked);
    connect(analyzeButton, &QPushButton::clicked, this, &DeviceListDialog::analyzeInstanceConflicts);
    connect(m_closeButton, &QPushButton::clicked, this, &DeviceListDialog::onCloseClicked);
    
    buttonLayout->addWidget(m_refreshButton);
    buttonLayout->addWidget(analyzeButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_closeButton);
    
    mainLayout->addLayout(buttonLayout);
}

void DeviceListDialog::updateDeviceList()
{
    if (!nmea2000) {
        m_statusLabel->setText("NMEA2000 interface not initialized");
        m_statusLabel->setStyleSheet("font-weight: bold; color: red;");
        return;
    }
    
    populateDeviceTable();
}

void DeviceListDialog::populateDeviceTable()
{
    // Clear existing rows
    m_deviceTable->setRowCount(0);
    
    if (!m_deviceList) {
        m_statusLabel->setText("Device list not available");
        m_statusLabel->setStyleSheet("font-weight: bold; color: red;");
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
        m_statusLabel->setStyleSheet("font-weight: bold; color: orange;");
    } else {
        m_statusLabel->setText(QString("Found %1 NMEA2000 device(s) - Auto-updating every 2 seconds").arg(deviceCount));
        m_statusLabel->setStyleSheet("font-weight: bold; color: green;");
    }
    
    // Analyze and highlight instance conflicts
    highlightInstanceConflicts();
    
    // Resize columns to content
    m_deviceTable->resizeColumnsToContents();
}

void DeviceListDialog::highlightInstanceConflicts()
{
    if (!m_mainWindow) {
        return;
    }
    
    // Get real-time PGN instance conflicts from MainWindow
    QList<InstanceConflict> conflicts = m_mainWindow->getInstanceConflicts();
    QSet<uint8_t> conflictingSources = m_mainWindow->getConflictingSources();
    
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
    for (int row = 0; row < m_deviceTable->rowCount(); row++) {
        QTableWidgetItem* nodeItem = m_deviceTable->item(row, 0);
        if (nodeItem) {
            bool ok;
            uint8_t source = nodeItem->text().toUInt(&ok, 16);
            if (ok && conflictingSources.contains(source)) {
                // Highlight the row in red
                for (int col = 0; col < m_deviceTable->columnCount(); col++) {
                    QTableWidgetItem* item = m_deviceTable->item(row, col);
                    if (item) {
                        item->setBackground(QBrush(QColor(255, 200, 200))); // Light red
                    }
                }
            }
        }
    }
    
    // Update status with conflict information
    if (!conflicts.isEmpty()) {
        int deviceCount = m_deviceTable->rowCount();
        QString conflictDetails;
        
        for (const InstanceConflict& conflict : conflicts) {
            QString sources;
            QStringList sourceList;
            for (uint8_t source : conflict.conflictingSources) {
                sourceList.append(QString::number(source, 16).toUpper());
            }
            sources = sourceList.join(", ");
            
            QString pgnName = getPGNName(conflict.pgn);
            conflictDetails += QString("PGN %1 (%2) Instance %3: Sources [%4]; ")
                                 .arg(conflict.pgn)
                                 .arg(pgnName)
                                 .arg(conflict.instance)
                                 .arg(sources);
        }
        
        m_statusLabel->setText(QString("Found %1 NMEA2000 device(s) - Auto-updating every 2 seconds\n"
                                     "⚠ INSTANCE CONFLICTS DETECTED: %2 devices affected\n"
                                     "Conflicts: %3")
                                     .arg(deviceCount)
                                     .arg(conflictingSources.size())
                                     .arg(conflictDetails));
        m_statusLabel->setStyleSheet("font-weight: bold; color: red;");
    } else {
        int deviceCount = m_deviceTable->rowCount();
        m_statusLabel->setText(QString("Found %1 NMEA2000 device(s) - Auto-updating every 2 seconds\n"
                                     "✓ No PGN instance conflicts detected")
                                     .arg(deviceCount));
        m_statusLabel->setStyleSheet("font-weight: bold; color: green;");
    }
}

void DeviceListDialog::onRowSelectionChanged()
{
    if (!m_mainWindow) {
        return;
    }
    
    int currentRow = m_deviceTable->currentRow();
    if (currentRow < 0) {
        return;
    }
    
    // Get current conflict data from MainWindow
    QList<InstanceConflict> conflicts = m_mainWindow->getInstanceConflicts();
    QSet<uint8_t> conflictingSources = m_mainWindow->getConflictingSources();
    
    // First, reset all highlighting except for the base conflict highlighting (red)
    for (int row = 0; row < m_deviceTable->rowCount(); row++) {
        QTableWidgetItem* nodeItem = m_deviceTable->item(row, 0);
        if (nodeItem) {
            bool ok;
            uint8_t source = nodeItem->text().toUInt(&ok, 16);
            if (ok && conflictingSources.contains(source)) {
                // Keep red highlighting for conflicted devices
                for (int col = 0; col < m_deviceTable->columnCount(); col++) {
                    QTableWidgetItem* item = m_deviceTable->item(row, col);
                    if (item) {
                        item->setBackground(QBrush(QColor(255, 200, 200))); // Light red
                    }
                }
            } else {
                // Reset non-conflicted devices to default
                for (int col = 0; col < m_deviceTable->columnCount(); col++) {
                    QTableWidgetItem* item = m_deviceTable->item(row, col);
                    if (item) {
                        item->setBackground(QBrush()); // Default background
                    }
                }
            }
        }
    }
    
    // Check if the selected row has conflicts
    QTableWidgetItem* selectedNodeItem = m_deviceTable->item(currentRow, 0);
    if (selectedNodeItem) {
        bool ok;
        uint8_t selectedSource = selectedNodeItem->text().toUInt(&ok, 16);
        
        if (ok && conflictingSources.contains(selectedSource)) {
            // Find all conflicts involving this source and highlight related sources in orange
            for (const InstanceConflict& conflict : conflicts) {
                if (conflict.conflictingSources.contains(selectedSource)) {
                    // Highlight all sources in this conflict group in orange
                    for (uint8_t conflictSource : conflict.conflictingSources) {
                        // Find the row for this conflicting source
                        for (int row = 0; row < m_deviceTable->rowCount(); row++) {
                            QTableWidgetItem* nodeItem = m_deviceTable->item(row, 0);
                            if (nodeItem) {
                                bool rowOk;
                                uint8_t rowSource = nodeItem->text().toUInt(&rowOk, 16);
                                if (rowOk && rowSource == conflictSource) {
                                    // Highlight this row in orange
                                    for (int col = 0; col < m_deviceTable->columnCount(); col++) {
                                        QTableWidgetItem* item = m_deviceTable->item(row, col);
                                        if (item) {
                                            item->setBackground(QBrush(QColor(255, 180, 100))); // Orange
                                        }
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

QString DeviceListDialog::getDeviceClassName(unsigned char deviceClass)
{
    switch(deviceClass) {
        case 0: return "Reserved";
        case 10: return "System Tools";
        case 20: return "Safety";
        case 25: return "Internetwork Device";
        case 30: return "Electrical Distribution";
        case 35: return "Electrical Generation";
        case 40: return "Steering and Control";
        case 50: return "Propulsion";
        case 60: return "Navigation";
        case 70: return "Communication";
        case 75: return "Sensor Communication Interface";
        case 80: return "Instrumentation/General Systems";
        case 85: return "External Environment";
        case 90: return "Internal Environment";
        case 100: return "Deck + Cargo + Fishing Equipment";
        case 110: return "Display";
        case 120: return "Entertainment";
        default: return QString("Unknown (%1)").arg(deviceClass);
    }
}

QString DeviceListDialog::getManufacturerName(uint16_t manufacturerCode)
{
    switch(manufacturerCode) {
        case 147: return "Garmin";
        case 229: return "Garmin";       // You mentioned 229 is also Garmin
        case 137: return "Maretron";
        case 358: return "Victron";
        case 135: return "Airmar";       // You corrected this from 273
        case 273: return "Airmar";       // Keep both for compatibility
        case 176: return "Carling Technologies";
        case 504: return "Vesper";       // You corrected this from 1857
        case 1857: return "Vesper Marine"; // Keep original for compatibility
        case 78: return "Furuno";
        case 1863: return "Raymarine";
        case 215: return "B&G";
        case 1855: return "Simrad";
        case 304: return "Lowrance";
        case 529: return "Yacht Devices";
        case 2046: return "+2046";  // Based on screenshot
        case 1403: return "Arco Marine";  // Based on screenshot
        default: return QString("Unknown (%1)").arg(manufacturerCode);
    }
}

QString DeviceListDialog::getDeviceFunctionName(unsigned char deviceFunction)
{
    switch(deviceFunction) {
        case 0: return "Network Function";
        case 110: return "Display";
        case 120: return "Dedicated Display";
        case 130: return "Repeater Station";
        case 140: return "PC Gateway";
        case 150: return "Router";
        case 160: return "Bridge";
        case 170: return "Instrumentation";
        case 175: return "Observer";
        case 180: return "System Controller";
        default: return QString("Unknown (%1)").arg(deviceFunction);
    }
}

void DeviceListDialog::onRefreshClicked()
{
    updateDeviceList();
}

void DeviceListDialog::analyzeInstanceConflicts()
{
    if (!m_mainWindow) {
        QMessageBox::warning(this, "Analysis Error", "Main window reference not available for analysis.");
        return;
    }

    // Get real-time PGN instance conflicts from MainWindow
    QList<InstanceConflict> pgnConflicts = m_mainWindow->getInstanceConflicts();
    QSet<uint8_t> conflictingSources = m_mainWindow->getConflictingSources();
    
    QString analysisResult;
    
    if (pgnConflicts.isEmpty()) {
        analysisResult = QString("✓ ANALYSIS COMPLETE - No PGN instance conflicts detected!\n\n"
                               "Real-time monitoring has not detected any devices transmitting\n"
                               "the same PGN with identical instance numbers.\n\n"
                               "📋 MONITORED PGNs:\n"
                               "• PGN 127488 - Engine Parameters (Engine Instance)\n"
                               "• PGN 127502 - Binary Switch Bank Control (Instance)\n"
                               "• PGN 127505 - Fluid Level (Tank Instance)\n"
                               "• PGN 127508 - Battery Status (Battery Instance)\n"
                               "• PGN 130312 - Temperature (Sensor Instance)\n"
                               "• PGN 130314 - Actual Pressure (Sensor Instance)\n\n"
                               "⚠ NOTE: Conflicts are only detected when devices actively transmit data.\n"
                               "Make sure all devices are powered on and transmitting to get complete analysis.");
    } else {
        analysisResult = QString("⚠ REAL-TIME PGN INSTANCE CONFLICTS DETECTED ⚠\n\n"
                               "Found %1 PGN instance conflict(s) that WILL cause data corruption:\n\n")
                               .arg(pgnConflicts.size());
        
        for (const InstanceConflict& conflict : pgnConflicts) {
            QString pgnName = getPGNName(conflict.pgn);
            QString sources;
            QStringList sourceList;
            for (uint8_t source : conflict.conflictingSources) {
                sourceList.append(QString("Node %1").arg(source, 2, 16, QChar('0')).toUpper());
            }
            sources = sourceList.join(", ");
            
            analysisResult += QString("🔴 PGN %1 (%2) - Instance %3:\n"
                                    "   Conflicting sources: %4\n"
                                    "   ⚠ Data from these devices cannot be differentiated!\n\n")
                                    .arg(conflict.pgn)
                                    .arg(pgnName)
                                    .arg(conflict.instance)
                                    .arg(sources);
        }
        
        analysisResult += QString("📋 CRITICAL RECOMMENDATIONS:\n"
                                "• Instance conflicts cause real data corruption and confusion\n"
                                "• Each device must use a unique instance number for the same PGN\n"
                                "• Use manufacturer configuration tools to change instance numbers immediately\n"
                                "• For fluid levels: Tank 1=0, Tank 2=1, Tank 3=2, etc.\n"
                                "• For batteries: Battery 1=0, Battery 2=1, Battery 3=2, etc.\n"
                                "• For engines: Engine 1=0, Engine 2=1, etc.\n"
                                "• Test after changes to ensure conflicts are resolved");
    }
    
    // Show analysis in a dialog
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("NMEA 2000 Real-Time PGN Instance Conflict Analysis");
    msgBox.setText(analysisResult);
    msgBox.setIcon(pgnConflicts.isEmpty() ? QMessageBox::Information : QMessageBox::Critical);
    msgBox.setStandardButtons(QMessageBox::Ok);
    
    // Make the dialog wider for better readability
    msgBox.setStyleSheet("QMessageBox { min-width: 600px; }");
    msgBox.exec();
}

void DeviceListDialog::onCloseClicked()
{
    accept();
}

QString DeviceListDialog::getPGNName(unsigned long pgn)
{
    switch(pgn) {
        case 127245: return "Rudder";
        case 127250: return "Vessel Heading";
        case 127251: return "Rate of Turn";
        case 127488: return "Engine Parameters, Rapid";
        case 127502: return "Binary Switch Bank Control";
        case 127505: return "Fluid Level";
        case 127508: return "Battery Status";
        case 128259: return "Boat Speed";
        case 128267: return "Water Depth";
        case 129025: return "Position Rapid";
        case 129026: return "COG & SOG Rapid";
        case 129029: return "GNSS Position";
        case 130306: return "Wind Data";
        case 130310: return "Environmental Parameters";
        case 130312: return "Temperature";
        case 130314: return "Actual Pressure";
        default: return QString("PGN %1").arg(pgn);
    }
}

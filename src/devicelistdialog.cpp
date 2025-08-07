#include "devicelistdialog.h"
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
    // Clear previous conflict data
    m_conflictGroups.clear();
    m_conflictingSources.clear();
    
    if (!m_deviceList) {
        return;
    }
    
    // Structure to track device instances by class and function
    struct DeviceInfo {
        uint8_t source;
        uint8_t instance;
        uint8_t deviceClass;
        uint8_t deviceFunction;
        int tableRow;
    };
    
    QList<DeviceInfo> devices;
    
    // Collect all device information and find their table rows
    for (int row = 0; row < m_deviceTable->rowCount(); row++) {
        QTableWidgetItem* nodeItem = m_deviceTable->item(row, 0);
        if (nodeItem) {
            bool ok;
            uint8_t source = nodeItem->text().toUInt(&ok, 16);
            if (ok) {
                const tNMEA2000::tDevice* device = m_deviceList->FindDeviceBySource(source);
                if (device) {
                    DeviceInfo info;
                    info.source = source;
                    info.instance = device->GetDeviceInstance();
                    info.deviceClass = device->GetDeviceClass();
                    info.deviceFunction = device->GetDeviceFunction();
                    info.tableRow = row;
                    devices.append(info);
                }
            }
        }
    }
    
    // Group devices by class and function, then check for instance conflicts
    QMap<QString, QList<DeviceInfo>> deviceGroups;
    
    for (const DeviceInfo& device : devices) {
        QString groupKey = QString("%1_%2").arg(device.deviceClass).arg(device.deviceFunction);
        deviceGroups[groupKey].append(device);
    }
    
    // Find conflicts and reset all row colors first
    for (int row = 0; row < m_deviceTable->rowCount(); row++) {
        for (int col = 0; col < m_deviceTable->columnCount(); col++) {
            QTableWidgetItem* item = m_deviceTable->item(row, col);
            if (item) {
                item->setBackground(QBrush()); // Reset to default background
            }
        }
    }
    
    // Check each group for instance conflicts
    for (auto it = deviceGroups.begin(); it != deviceGroups.end(); ++it) {
        const QList<DeviceInfo>& group = it.value();
        
        if (group.size() > 1) {
            // Check for duplicate instances within the same class/function group
            QMap<uint8_t, QList<DeviceInfo>> instanceMap;
            
            for (const DeviceInfo& device : group) {
                instanceMap[device.instance].append(device);
            }
            
            // Highlight conflicts
            for (auto instIt = instanceMap.begin(); instIt != instanceMap.end(); ++instIt) {
                const QList<DeviceInfo>& instanceDevices = instIt.value();
                
                if (instanceDevices.size() > 1) {
                    // This is a conflict - collect all conflicting sources
                    QList<uint8_t> conflictGroup;
                    for (const DeviceInfo& conflictDevice : instanceDevices) {
                        conflictGroup.append(conflictDevice.source);
                        m_conflictingSources.insert(conflictDevice.source);
                        
                        // Highlight the row in red
                        for (int col = 0; col < m_deviceTable->columnCount(); col++) {
                            QTableWidgetItem* item = m_deviceTable->item(conflictDevice.tableRow, col);
                            if (item) {
                                item->setBackground(QBrush(QColor(255, 200, 200))); // Light red
                            }
                        }
                    }
                    
                    // Store the conflict group for each source
                    for (uint8_t source : conflictGroup) {
                        m_conflictGroups[source] = conflictGroup;
                    }
                }
            }
        }
    }
    
    // Update status to include conflict information
    if (!m_conflictingSources.isEmpty()) {
        int deviceCount = m_deviceTable->rowCount();
        m_statusLabel->setText(QString("Found %1 NMEA2000 device(s) - Auto-updating every 2 seconds\n"
                                     "⚠ %2 devices have instance conflicts (highlighted in red). Click a conflicted device to see all related conflicts.")
                                     .arg(deviceCount).arg(m_conflictingSources.size()));
        m_statusLabel->setStyleSheet("font-weight: bold; color: red;");
    }
}

void DeviceListDialog::onRowSelectionChanged()
{
    int currentRow = m_deviceTable->currentRow();
    if (currentRow < 0) {
        return;
    }
    
    // First, reset all highlighting except for the base conflict highlighting (red)
    for (int row = 0; row < m_deviceTable->rowCount(); row++) {
        QTableWidgetItem* nodeItem = m_deviceTable->item(row, 0);
        if (nodeItem) {
            bool ok;
            uint8_t source = nodeItem->text().toUInt(&ok, 16);
            if (ok && m_conflictingSources.contains(source)) {
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
        if (ok && m_conflictGroups.contains(selectedSource)) {
            // Highlight all devices in the same conflict group in orange
            const QList<uint8_t>& conflictGroup = m_conflictGroups[selectedSource];
            
            for (uint8_t conflictSource : conflictGroup) {
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
    if (!m_deviceList) {
        QMessageBox::warning(this, "Analysis Error", "Device list not available for analysis.");
        return;
    }

    // Structure to track device instances by class and function
    struct DeviceInfo {
        uint8_t source;
        uint8_t instance;
        uint8_t deviceClass;
        uint8_t deviceFunction;
        QString manufacturer;
        QString modelId;
    };
    
    QList<DeviceInfo> devices;
    QStringList conflicts;
    
    // Collect all device information
    for (uint8_t source = 0; source < N2kMaxBusDevices; source++) {
        const tNMEA2000::tDevice* device = m_deviceList->FindDeviceBySource(source);
        if (device) {
            DeviceInfo info;
            info.source = source;
            info.instance = device->GetDeviceInstance();
            info.deviceClass = device->GetDeviceClass();
            info.deviceFunction = device->GetDeviceFunction();
            info.manufacturer = getManufacturerName(device->GetManufacturerCode());
            
            const char* modelIdStr = device->GetModelID();
            info.modelId = (modelIdStr && strlen(modelIdStr) > 0) ? QString(modelIdStr) : "Unknown";
            
            devices.append(info);
        }
    }
    
    // Analyze for conflicts
    // Group devices by class and function, then check for instance conflicts
    QMap<QString, QList<DeviceInfo>> deviceGroups;
    
    for (const DeviceInfo& device : devices) {
        QString groupKey = QString("%1_%2").arg(device.deviceClass).arg(device.deviceFunction);
        deviceGroups[groupKey].append(device);
    }
    
    // Check each group for instance conflicts
    for (auto it = deviceGroups.begin(); it != deviceGroups.end(); ++it) {
        const QList<DeviceInfo>& group = it.value();
        
        if (group.size() > 1) {
            // Check for duplicate instances within the same class/function group
            QMap<uint8_t, QList<DeviceInfo>> instanceMap;
            
            for (const DeviceInfo& device : group) {
                instanceMap[device.instance].append(device);
            }
            
            // Report conflicts
            for (auto instIt = instanceMap.begin(); instIt != instanceMap.end(); ++instIt) {
                const QList<DeviceInfo>& instanceDevices = instIt.value();
                
                if (instanceDevices.size() > 1) {
                    QString deviceClassName = getDeviceClassName(instanceDevices[0].deviceClass);
                    QString deviceFunctionName = getDeviceFunctionName(instanceDevices[0].deviceFunction);
                    
                    QString conflictMsg = QString("INSTANCE CONFLICT - %1/%2 (Instance %3):")
                        .arg(deviceClassName)
                        .arg(deviceFunctionName)
                        .arg(instanceDevices[0].instance);
                    
                    for (const DeviceInfo& conflictDevice : instanceDevices) {
                        conflictMsg += QString("\n  • Node %1 (%2 %3)")
                            .arg(conflictDevice.source, 2, 16, QChar('0')).toUpper()
                            .arg(conflictDevice.manufacturer)
                            .arg(conflictDevice.modelId);
                    }
                    
                    conflicts.append(conflictMsg);
                }
            }
        }
    }
    
    // Additional analysis for specific PGN conflicts (common problematic PGNs)
    QStringList criticalPGNConflicts;
    
    // Check for devices that might transmit the same critical PGNs
    QList<DeviceInfo> tankDevices;
    QList<DeviceInfo> batteryDevices;
    QList<DeviceInfo> engineDevices;
    QList<DeviceInfo> navigationDevices;
    
    for (const DeviceInfo& device : devices) {
        // Tank level devices (class 30 = Electrical Distribution, but also check others)
        if (device.deviceClass == 30 || device.deviceClass == 80) {
            tankDevices.append(device);
        }
        // Battery monitoring (class 30 = Electrical Distribution)
        if (device.deviceClass == 30 || device.deviceClass == 35) {
            batteryDevices.append(device);
        }
        // Engine data (class 50 = Propulsion)
        if (device.deviceClass == 50) {
            engineDevices.append(device);
        }
        // Navigation data (class 60 = Navigation)
        if (device.deviceClass == 60) {
            navigationDevices.append(device);
        }
    }
    
    // Display results
    QString analysisResult;
    
    if (conflicts.isEmpty() && criticalPGNConflicts.isEmpty()) {
        analysisResult = QString("✓ ANALYSIS COMPLETE - No instance conflicts detected!\n\n"
                               "Analyzed %1 devices across %2 device groups.\n"
                               "All device instances appear to be properly configured.")
                               .arg(devices.size())
                               .arg(deviceGroups.size());
    } else {
        analysisResult = QString("⚠ INSTANCE CONFLICTS DETECTED ⚠\n\n"
                               "Found %1 instance conflict(s) that may cause data corruption:\n\n")
                               .arg(conflicts.size());
        
        analysisResult += conflicts.join("\n\n");
        
        analysisResult += QString("\n\n📋 RECOMMENDATIONS:\n"
                                "• Each device of the same type should have a unique instance number\n"
                                "• Use manufacturer configuration tools to change instance numbers\n"
                                "• Critical for: Tank levels, Battery monitoring, Engine data, Navigation data\n"
                                "• Instance conflicts can cause intermittent or incorrect readings");
    }
    
    // Show analysis in a dialog
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("NMEA 2000 Instance Conflict Analysis");
    msgBox.setText(analysisResult);
    msgBox.setIcon(conflicts.isEmpty() ? QMessageBox::Information : QMessageBox::Warning);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
}

void DeviceListDialog::onCloseClicked()
{
    accept();
}

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
            << "Device Instance" << "Label" << "Current Software" << "Installation Description";
    m_deviceTable->setHorizontalHeaderLabels(headers);
    
    // Configure table
    m_deviceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_deviceTable->setAlternatingRowColors(true);
    m_deviceTable->horizontalHeader()->setStretchLastSection(true);
    m_deviceTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_deviceTable->verticalHeader()->setVisible(false);
    m_deviceTable->setSortingEnabled(true);
    
    mainLayout->addWidget(m_deviceTable);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    m_refreshButton = new QPushButton("Refresh Now");
    m_closeButton = new QPushButton("Close");
    
    connect(m_refreshButton, &QPushButton::clicked, this, &DeviceListDialog::onRefreshClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &DeviceListDialog::onCloseClicked);
    
    buttonLayout->addWidget(m_refreshButton);
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
            
            // Label - create based on device function and class
            QString label = getDeviceFunctionName(device->GetDeviceFunction());
            if (label.startsWith("Unknown")) {
                label = getDeviceClassName(device->GetDeviceClass());
            }
            if (label.startsWith("Unknown")) {
                label = QString("Device %1").arg(source, 2, 16, QChar('0')).toUpper();
            }
            m_deviceTable->setItem(deviceCount, 5, new QTableWidgetItem(label));
            
            // Current Software - use virtual method
            QString softwareVersion = "-";
            const char* swCodeStr = device->GetSwCode();
            if (swCodeStr && strlen(swCodeStr) > 0) {
                softwareVersion = QString(swCodeStr);
            }
            m_deviceTable->setItem(deviceCount, 6, new QTableWidgetItem(softwareVersion));
            
            // Installation Description - placeholder for now
            QString installDesc = "-";
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
    
    // Resize columns to content
    m_deviceTable->resizeColumnsToContents();
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

void DeviceListDialog::onCloseClicked()
{
    accept();
}

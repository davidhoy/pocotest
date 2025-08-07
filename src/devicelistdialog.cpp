#include "devicelistdialog.h"
#include "NMEA2000_SocketCAN.h"
#include <QMessageBox>
#include <QApplication>

extern tNMEA2000_SocketCAN* nmea2000;

DeviceListDialog::DeviceListDialog(QWidget *parent)
    : QDialog(parent)
    , m_deviceTable(nullptr)
    , m_refreshButton(nullptr)
    , m_closeButton(nullptr)
    , m_statusLabel(nullptr)
    , m_updateTimer(nullptr)
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
    m_deviceTable->setColumnCount(7);
    
    QStringList headers;
    headers << "Source" << "Name" << "Model ID" << "SW Code" << "Model Version" 
            << "Device Class" << "Device Function";
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
    
    if (!nmea2000) {
        return;
    }
    
    int deviceCount = 0;
    
    // Check each possible source address (0-251)
    for (uint8_t source = 0; source <= 251; source++) {
        // For now, create a simplified device table showing active sources
        // This is a basic implementation - a more complete version would need
        // access to the internal device list
        
        // Add a placeholder entry to demonstrate the functionality
        if (source == 22) { // Our default source address
            m_deviceTable->insertRow(deviceCount);
            
            // Source
            QTableWidgetItem* sourceItem = new QTableWidgetItem(QString::number(source));
            sourceItem->setTextAlignment(Qt::AlignCenter);
            m_deviceTable->setItem(deviceCount, 0, sourceItem);
            
            // Name
            m_deviceTable->setItem(deviceCount, 1, new QTableWidgetItem("Local Device"));
            
            // Model ID
            QTableWidgetItem* modelItem = new QTableWidgetItem("Unknown");
            modelItem->setTextAlignment(Qt::AlignCenter);
            m_deviceTable->setItem(deviceCount, 2, modelItem);
            
            // SW Code
            QTableWidgetItem* swItem = new QTableWidgetItem("Unknown");
            swItem->setTextAlignment(Qt::AlignCenter);
            m_deviceTable->setItem(deviceCount, 3, swItem);
            
            // Model Version
            QTableWidgetItem* versionItem = new QTableWidgetItem("Unknown");
            m_deviceTable->setItem(deviceCount, 4, versionItem);
            
            // Device Class
            QString deviceClass = "PC Gateway";
            m_deviceTable->setItem(deviceCount, 5, new QTableWidgetItem(deviceClass));
            
            // Device Function
            QString deviceFunction = "PC Gateway";
            m_deviceTable->setItem(deviceCount, 6, new QTableWidgetItem(deviceFunction));
            
            deviceCount++;
        }
    }
    
    // Update status
    if (deviceCount == 0) {
        m_statusLabel->setText("No NMEA2000 devices detected");
        m_statusLabel->setStyleSheet("font-weight: bold; color: orange;");
    } else {
        m_statusLabel->setText(QString("Found %1 NMEA2000 device(s) - Auto-updating every 2 seconds\n\nNote: This is a basic device list implementation. For full device discovery with product information, model IDs, and configuration data, deeper integration with the NMEA2000 device list handler would be required.").arg(deviceCount));
        m_statusLabel->setStyleSheet("font-weight: bold; color: green;");
    }
    
    // Resize columns to content
    m_deviceTable->resizeColumnsToContents();
}

QString DeviceListDialog::getDeviceClassName(unsigned long deviceClass)
{
    switch (deviceClass) {
        case 25: return "Network Interconnect";
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
        case 120: return "Safety";
        case 125: return "Internetwork Device";
        default: return QString("Unknown (%1)").arg(deviceClass);
    }
}

QString DeviceListDialog::getDeviceFunctionName(unsigned char deviceFunction)
{
    switch (deviceFunction) {
        case 130: return "PC Gateway";
        case 140: return "Engine";
        case 150: return "Propulsion";
        case 160: return "Navigation";
        case 170: return "Communication";
        case 180: return "Sensor Communication Interface";
        case 190: return "Instrumentation/General Systems";
        case 200: return "External Environment";
        case 210: return "Internal Environment";
        case 220: return "Deck + Cargo + Fishing Equipment";
        case 230: return "Safety";
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

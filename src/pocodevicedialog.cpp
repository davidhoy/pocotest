#include "pocodevicedialog.h"
#include "LumitecPoco.h"
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QMessageBox>

PocoDeviceDialog::PocoDeviceDialog(uint8_t deviceAddress, const QString& deviceName, QWidget *parent)
    : QDialog(parent)
    , m_deviceAddress(deviceAddress)
    , m_deviceName(deviceName)
    , m_tabWidget(nullptr)
    , m_switchControlTab(nullptr)
    , m_switchIdSpinBox(nullptr)
    , m_onButton(nullptr)
    , m_offButton(nullptr)
    , m_whiteButton(nullptr)
    , m_redButton(nullptr)
    , m_greenButton(nullptr)
    , m_blueButton(nullptr)
    , m_colorControlTab(nullptr)
    , m_colorControlButton(nullptr)
    , m_deviceInfoTab(nullptr)
    , m_deviceAddressLabel(nullptr)
    , m_deviceNameLabel(nullptr)
    , m_queryDeviceButton(nullptr)
    , m_buttonBox(nullptr)
{
    setupUI();
    setModal(true);
    setWindowTitle(QString("Poco Device Control - %1 (0x%2)")
                   .arg(m_deviceName)
                   .arg(m_deviceAddress, 2, 16, QChar('0')));
    resize(400, 300);
}

PocoDeviceDialog::~PocoDeviceDialog()
{
    // Qt will handle cleanup of child widgets
}

void PocoDeviceDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Create tab widget
    m_tabWidget = new QTabWidget();
    mainLayout->addWidget(m_tabWidget);
    
    // Setup tabs
    setupSwitchControlTab();
    setupColorControlTab();
    setupDeviceInfoTab();
    
    // Add dialog buttons
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(m_buttonBox);
}

void PocoDeviceDialog::setupSwitchControlTab()
{
    m_switchControlTab = new QWidget();
    m_tabWidget->addTab(m_switchControlTab, "Switch Control");
    
    QVBoxLayout* layout = new QVBoxLayout(m_switchControlTab);
    
    // Switch ID selection
    QGroupBox* switchGroup = new QGroupBox("Switch Selection");
    QHBoxLayout* switchLayout = new QHBoxLayout(switchGroup);
    
    switchLayout->addWidget(new QLabel("Switch ID:"));
    m_switchIdSpinBox = new QSpinBox();
    m_switchIdSpinBox->setRange(1, 16);
    m_switchIdSpinBox->setValue(1);
    switchLayout->addWidget(m_switchIdSpinBox);
    switchLayout->addStretch();
    
    layout->addWidget(switchGroup);
    
    // Action buttons
    QGroupBox* actionGroup = new QGroupBox("Actions");
    QVBoxLayout* actionLayout = new QVBoxLayout(actionGroup);
    
    // First row of buttons: On/Off
    QHBoxLayout* row1Layout = new QHBoxLayout();
    m_onButton = new QPushButton("On");
    m_onButton->setProperty("actionId", ACTION_ON);
    connect(m_onButton, &QPushButton::clicked, this, &PocoDeviceDialog::onActionButtonClicked);
    row1Layout->addWidget(m_onButton);
    
    m_offButton = new QPushButton("Off");
    m_offButton->setProperty("actionId", ACTION_OFF);
    connect(m_offButton, &QPushButton::clicked, this, &PocoDeviceDialog::onActionButtonClicked);
    row1Layout->addWidget(m_offButton);
    
    actionLayout->addLayout(row1Layout);
    
    // Second row of buttons: Colors
    QHBoxLayout* row2Layout = new QHBoxLayout();
    m_whiteButton = new QPushButton("White");
    m_whiteButton->setProperty("actionId", ACTION_WHITE);
    connect(m_whiteButton, &QPushButton::clicked, this, &PocoDeviceDialog::onActionButtonClicked);
    row2Layout->addWidget(m_whiteButton);
    
    m_redButton = new QPushButton("Red");
    m_redButton->setProperty("actionId", ACTION_RED);
    connect(m_redButton, &QPushButton::clicked, this, &PocoDeviceDialog::onActionButtonClicked);
    row2Layout->addWidget(m_redButton);
    
    actionLayout->addLayout(row2Layout);
    
    // Third row of buttons: More colors
    QHBoxLayout* row3Layout = new QHBoxLayout();
    m_greenButton = new QPushButton("Green");
    m_greenButton->setProperty("actionId", ACTION_GREEN);
    connect(m_greenButton, &QPushButton::clicked, this, &PocoDeviceDialog::onActionButtonClicked);
    row3Layout->addWidget(m_greenButton);
    
    m_blueButton = new QPushButton("Blue");
    m_blueButton->setProperty("actionId", ACTION_BLUE);
    connect(m_blueButton, &QPushButton::clicked, this, &PocoDeviceDialog::onActionButtonClicked);
    row3Layout->addWidget(m_blueButton);
    
    actionLayout->addLayout(row3Layout);
    
    layout->addWidget(actionGroup);
    layout->addStretch();
}

void PocoDeviceDialog::setupColorControlTab()
{
    m_colorControlTab = new QWidget();
    m_tabWidget->addTab(m_colorControlTab, "Color Control");
    
    QVBoxLayout* layout = new QVBoxLayout(m_colorControlTab);
    
    QGroupBox* colorGroup = new QGroupBox("Advanced Color Control");
    QVBoxLayout* colorLayout = new QVBoxLayout(colorGroup);
    
    QLabel* infoLabel = new QLabel("Advanced color control features will be added here.\n"
                                   "This will include HSB sliders, pattern controls, etc.");
    infoLabel->setWordWrap(true);
    colorLayout->addWidget(infoLabel);
    
    m_colorControlButton = new QPushButton("Open Color Control Dialog");
    connect(m_colorControlButton, &QPushButton::clicked, this, &PocoDeviceDialog::onColorControlTriggered);
    colorLayout->addWidget(m_colorControlButton);
    
    layout->addWidget(colorGroup);
    layout->addStretch();
}

void PocoDeviceDialog::setupDeviceInfoTab()
{
    m_deviceInfoTab = new QWidget();
    m_tabWidget->addTab(m_deviceInfoTab, "Device Info");
    
    QVBoxLayout* layout = new QVBoxLayout(m_deviceInfoTab);
    
    QGroupBox* infoGroup = new QGroupBox("Device Information");
    QVBoxLayout* infoLayout = new QVBoxLayout(infoGroup);
    
    m_deviceAddressLabel = new QLabel(QString("Address: 0x%1").arg(m_deviceAddress, 2, 16, QChar('0')));
    m_deviceNameLabel = new QLabel(QString("Name: %1").arg(m_deviceName));
    
    infoLayout->addWidget(m_deviceAddressLabel);
    infoLayout->addWidget(m_deviceNameLabel);
    
    m_queryDeviceButton = new QPushButton("Query Device Information");
    connect(m_queryDeviceButton, &QPushButton::clicked, this, &PocoDeviceDialog::onDeviceInfoRequested);
    infoLayout->addWidget(m_queryDeviceButton);
    
    layout->addWidget(infoGroup);
    layout->addStretch();
}

void PocoDeviceDialog::onActionButtonClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button) {
        return;
    }
    
    uint8_t switchId = static_cast<uint8_t>(m_switchIdSpinBox->value());
    uint8_t actionId = static_cast<uint8_t>(button->property("actionId").toInt());
    
    emit switchActionRequested(m_deviceAddress, switchId, actionId);
}

void PocoDeviceDialog::onColorControlTriggered()
{
    emit colorControlRequested(m_deviceAddress);
    
    // For now, just show a placeholder message
    QMessageBox::information(this, "Color Control", 
                           "Advanced color control dialog will be opened.\n"
                           "This feature will be implemented later.");
}

void PocoDeviceDialog::onDeviceInfoRequested()
{
    emit deviceInfoRequested(m_deviceAddress);
    
    // Show confirmation
    QMessageBox::information(this, "Device Query", 
                           QString("Device information query sent to device 0x%1")
                           .arg(m_deviceAddress, 2, 16, QChar('0')));
}

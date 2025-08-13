#include "pocodevicedialog.h"
#include "LumitecPoco.h"
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QComboBox>
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
    , m_actionComboBox(nullptr)
    , m_sendActionButton(nullptr)
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
    
    // Action selection
    QGroupBox* actionGroup = new QGroupBox("Action Selection");
    QVBoxLayout* actionLayout = new QVBoxLayout(actionGroup);
    
    QHBoxLayout* comboLayout = new QHBoxLayout();
    comboLayout->addWidget(new QLabel("Action:"));
    m_actionComboBox = new QComboBox();
    m_actionComboBox->addItem("Turn Light On", ACTION_ON);
    m_actionComboBox->addItem("Turn Light Off", ACTION_OFF);
    m_actionComboBox->addItem("Set White", ACTION_WHITE);
    m_actionComboBox->addItem("Set Red", ACTION_RED);
    m_actionComboBox->addItem("Set Green", ACTION_GREEN);
    m_actionComboBox->addItem("Set Blue", ACTION_BLUE);
    comboLayout->addWidget(m_actionComboBox);
    comboLayout->addStretch();
    
    actionLayout->addLayout(comboLayout);
    
    m_sendActionButton = new QPushButton("Send Action");
    connect(m_sendActionButton, &QPushButton::clicked, this, &PocoDeviceDialog::onSwitchActionTriggered);
    actionLayout->addWidget(m_sendActionButton);
    
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

void PocoDeviceDialog::onSwitchActionTriggered()
{
    uint8_t switchId = static_cast<uint8_t>(m_switchIdSpinBox->value());
    uint8_t actionId = static_cast<uint8_t>(m_actionComboBox->currentData().toInt());
    
    emit switchActionRequested(m_deviceAddress, switchId, actionId);
    
    // Show confirmation
    QString actionName = m_actionComboBox->currentText();
    QMessageBox::information(this, "Action Sent", 
                           QString("Sent action '%1' to switch %2 on device 0x%3")
                           .arg(actionName)
                           .arg(switchId)
                           .arg(m_deviceAddress, 2, 16, QChar('0')));
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

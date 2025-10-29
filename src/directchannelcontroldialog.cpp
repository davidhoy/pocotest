#include "directchannelcontroldialog.h"
#include <QApplication>
#include <QIntValidator>
#include <QRegularExpressionValidator>

DirectChannelControlDialog::DirectChannelControlDialog(uint8_t deviceAddress, const QString& deviceName, QWidget *parent)
    : QDialog(parent)
    , m_deviceAddress(deviceAddress)
    , m_deviceName(deviceName)
{
    setWindowTitle(QString("Direct Channel Control - %1 (0x%2)")
                   .arg(deviceName)
                   .arg(deviceAddress, 2, 16, QChar('0')).toUpper());
    setModal(true);
    setupUI();
    resize(500, 400);
}

DirectChannelControlDialog::~DirectChannelControlDialog()
{
}

void DirectChannelControlDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Device info label
    QLabel* infoLabel = new QLabel(QString("Testing direct channel control messages for device %1").arg(m_deviceName));
    infoLabel->setWordWrap(true);
    mainLayout->addWidget(infoLabel);

    // Tab widget
    m_tabWidget = new QTabWidget(this);
    mainLayout->addWidget(m_tabWidget);

    // Setup tabs
    setupBinTab();
    setupPwmTab();
    setupPliTab();
    setupPliT2HSBTab();

    // Button box
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(m_buttonBox);
}

void DirectChannelControlDialog::setupBinTab()
{
    m_binTab = new QWidget();
    m_tabWidget->addTab(m_binTab, "BIN Control");

    QVBoxLayout* layout = new QVBoxLayout(m_binTab);

    // Channel selection
    QHBoxLayout* channelLayout = new QHBoxLayout();
    channelLayout->addWidget(new QLabel("Channel:"));
    m_binChannelSpinBox = new QSpinBox();
    m_binChannelSpinBox->setRange(1, 255);
    m_binChannelSpinBox->setValue(1);
    channelLayout->addWidget(m_binChannelSpinBox);
    channelLayout->addStretch();
    layout->addLayout(channelLayout);

    // State selection
    QHBoxLayout* stateLayout = new QHBoxLayout();
    stateLayout->addWidget(new QLabel("State:"));
    m_binStateComboBox = new QComboBox();
    m_binStateComboBox->addItem("Off (0)", 0);
    m_binStateComboBox->addItem("On (1)", 1);
    stateLayout->addWidget(m_binStateComboBox);
    stateLayout->addStretch();
    layout->addLayout(stateLayout);

    layout->addStretch();

    // Send button
    m_sendBinButton = new QPushButton("Send BIN Control");
    connect(m_sendBinButton, &QPushButton::clicked, this, &DirectChannelControlDialog::onSendBinControl);
    layout->addWidget(m_sendBinButton);
}

void DirectChannelControlDialog::setupPwmTab()
{
    m_pwmTab = new QWidget();
    m_tabWidget->addTab(m_pwmTab, "PWM Control");

    QVBoxLayout* layout = new QVBoxLayout(m_pwmTab);

    // Channel selection
    QHBoxLayout* channelLayout = new QHBoxLayout();
    channelLayout->addWidget(new QLabel("Channel:"));
    m_pwmChannelSpinBox = new QSpinBox();
    m_pwmChannelSpinBox->setRange(1, 255);
    m_pwmChannelSpinBox->setValue(1);
    channelLayout->addWidget(m_pwmChannelSpinBox);
    channelLayout->addStretch();
    layout->addLayout(channelLayout);

    // Duty cycle
    QHBoxLayout* dutyLayout = new QHBoxLayout();
    dutyLayout->addWidget(new QLabel("Duty Cycle:"));
    m_pwmDutySlider = new QSlider(Qt::Horizontal);
    m_pwmDutySlider->setRange(0, 255);
    m_pwmDutySlider->setValue(128);
    m_pwmDutyValueLabel = new QLabel("128");
    connect(m_pwmDutySlider, &QSlider::valueChanged, [this](int value) {
        m_pwmDutyValueLabel->setText(QString::number(value));
    });
    dutyLayout->addWidget(m_pwmDutySlider);
    dutyLayout->addWidget(m_pwmDutyValueLabel);
    layout->addLayout(dutyLayout);

    // Transition time
    QHBoxLayout* transitionLayout = new QHBoxLayout();
    transitionLayout->addWidget(new QLabel("Transition Time (ms):"));
    m_pwmTransitionTimeSpinBox = new QSpinBox();
    m_pwmTransitionTimeSpinBox->setRange(0, 65535);
    m_pwmTransitionTimeSpinBox->setValue(1000);
    transitionLayout->addWidget(m_pwmTransitionTimeSpinBox);
    transitionLayout->addStretch();
    layout->addLayout(transitionLayout);

    layout->addStretch();

    // Send button
    m_sendPwmButton = new QPushButton("Send PWM Control");
    connect(m_sendPwmButton, &QPushButton::clicked, this, &DirectChannelControlDialog::onSendPwmControl);
    layout->addWidget(m_sendPwmButton);
}

void DirectChannelControlDialog::setupPliTab()
{
    m_pliTab = new QWidget();
    m_tabWidget->addTab(m_pliTab, "PLI Control");

    QVBoxLayout* layout = new QVBoxLayout(m_pliTab);

    // Channel selection
    QHBoxLayout* channelLayout = new QHBoxLayout();
    channelLayout->addWidget(new QLabel("Channel:"));
    m_pliChannelSpinBox = new QSpinBox();
    m_pliChannelSpinBox->setRange(1, 255);
    m_pliChannelSpinBox->setValue(1);
    channelLayout->addWidget(m_pliChannelSpinBox);
    channelLayout->addStretch();
    layout->addLayout(channelLayout);

    // PLI Message (Hex)
    QHBoxLayout* hexLayout = new QHBoxLayout();
    hexLayout->addWidget(new QLabel("PLI Message (Hex):"));
    m_pliMessageHexEdit = new QLineEdit("00000000");
    m_pliMessageHexEdit->setMaxLength(8);
    m_pliMessageHexEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9A-Fa-f]{0,8}")));
    connect(m_pliMessageHexEdit, &QLineEdit::textChanged, this, &DirectChannelControlDialog::updatePliHexDisplay);
    hexLayout->addWidget(m_pliMessageHexEdit);
    layout->addLayout(hexLayout);

    // PLI Message (Decimal)
    QHBoxLayout* decLayout = new QHBoxLayout();
    decLayout->addWidget(new QLabel("PLI Message (Dec):"));
    m_pliMessageDecEdit = new QLineEdit("0");
    m_pliMessageDecEdit->setValidator(new QIntValidator(0, 4294967295));
    connect(m_pliMessageDecEdit, &QLineEdit::textChanged, this, &DirectChannelControlDialog::updatePliHexDisplay);
    decLayout->addWidget(m_pliMessageDecEdit);
    layout->addLayout(decLayout);

    layout->addStretch();

    // Send button
    m_sendPliButton = new QPushButton("Send PLI Control");
    connect(m_sendPliButton, &QPushButton::clicked, this, &DirectChannelControlDialog::onSendPliControl);
    layout->addWidget(m_sendPliButton);
}

void DirectChannelControlDialog::setupPliT2HSBTab()
{
    m_pliT2HSBTab = new QWidget();
    m_tabWidget->addTab(m_pliT2HSBTab, "PLI T2HSB Control");

    QVBoxLayout* layout = new QVBoxLayout(m_pliT2HSBTab);

    // Channel selection
    QHBoxLayout* channelLayout = new QHBoxLayout();
    channelLayout->addWidget(new QLabel("Channel:"));
    m_pliT2HSBChannelSpinBox = new QSpinBox();
    m_pliT2HSBChannelSpinBox->setRange(1, 255);
    m_pliT2HSBChannelSpinBox->setValue(1);
    channelLayout->addWidget(m_pliT2HSBChannelSpinBox);
    channelLayout->addStretch();
    layout->addLayout(channelLayout);

    // PLI Clan
    QHBoxLayout* clanLayout = new QHBoxLayout();
    clanLayout->addWidget(new QLabel("PLI Clan:"));
    m_pliClanSpinBox = new QSpinBox();
    m_pliClanSpinBox->setRange(0, 255);
    m_pliClanSpinBox->setValue(0);
    clanLayout->addWidget(m_pliClanSpinBox);
    clanLayout->addStretch();
    layout->addLayout(clanLayout);

    // Transition
    QHBoxLayout* transitionLayout = new QHBoxLayout();
    transitionLayout->addWidget(new QLabel("Transition:"));
    m_pliTransitionSpinBox = new QSpinBox();
    m_pliTransitionSpinBox->setRange(0, 255);
    m_pliTransitionSpinBox->setValue(0);
    transitionLayout->addWidget(m_pliTransitionSpinBox);
    transitionLayout->addStretch();
    layout->addLayout(transitionLayout);

    // Brightness
    QHBoxLayout* brightnessLayout = new QHBoxLayout();
    brightnessLayout->addWidget(new QLabel("Brightness:"));
    m_pliBrightnessSlider = new QSlider(Qt::Horizontal);
    m_pliBrightnessSlider->setRange(0, 255);
    m_pliBrightnessSlider->setValue(255);
    m_pliBrightnessValueLabel = new QLabel("255");
    connect(m_pliBrightnessSlider, &QSlider::valueChanged, [this](int value) {
        m_pliBrightnessValueLabel->setText(QString::number(value));
    });
    brightnessLayout->addWidget(m_pliBrightnessSlider);
    brightnessLayout->addWidget(m_pliBrightnessValueLabel);
    layout->addLayout(brightnessLayout);

    // Hue
    QHBoxLayout* hueLayout = new QHBoxLayout();
    hueLayout->addWidget(new QLabel("Hue:"));
    m_pliHueSlider = new QSlider(Qt::Horizontal);
    m_pliHueSlider->setRange(0, 255);
    m_pliHueSlider->setValue(128);
    m_pliHueValueLabel = new QLabel("128");
    connect(m_pliHueSlider, &QSlider::valueChanged, [this](int value) {
        m_pliHueValueLabel->setText(QString::number(value));
    });
    hueLayout->addWidget(m_pliHueSlider);
    hueLayout->addWidget(m_pliHueValueLabel);
    layout->addLayout(hueLayout);

    // Saturation
    QHBoxLayout* saturationLayout = new QHBoxLayout();
    saturationLayout->addWidget(new QLabel("Saturation:"));
    m_pliSaturationSlider = new QSlider(Qt::Horizontal);
    m_pliSaturationSlider->setRange(0, 255);
    m_pliSaturationSlider->setValue(255);
    m_pliSaturationValueLabel = new QLabel("255");
    connect(m_pliSaturationSlider, &QSlider::valueChanged, [this](int value) {
        m_pliSaturationValueLabel->setText(QString::number(value));
    });
    saturationLayout->addWidget(m_pliSaturationSlider);
    saturationLayout->addWidget(m_pliSaturationValueLabel);
    layout->addLayout(saturationLayout);

    layout->addStretch();

    // Send button
    m_sendPliT2HSBButton = new QPushButton("Send PLI T2HSB Control");
    connect(m_sendPliT2HSBButton, &QPushButton::clicked, this, &DirectChannelControlDialog::onSendPliT2HSBControl);
    layout->addWidget(m_sendPliT2HSBButton);
}

void DirectChannelControlDialog::onSendBinControl()
{
    uint8_t channel = m_binChannelSpinBox->value();
    uint8_t state = m_binStateComboBox->currentData().toUInt();
    emit binControlRequested(m_deviceAddress, channel, state);
}

void DirectChannelControlDialog::onSendPwmControl()
{
    uint8_t channel = m_pwmChannelSpinBox->value();
    uint8_t duty = m_pwmDutySlider->value();
    uint16_t transitionTime = m_pwmTransitionTimeSpinBox->value();
    emit pwmControlRequested(m_deviceAddress, channel, duty, transitionTime);
}

void DirectChannelControlDialog::onSendPliControl()
{
    uint8_t channel = m_pliChannelSpinBox->value();
    uint32_t pliMessage = m_pliMessageDecEdit->text().toUInt();
    emit pliControlRequested(m_deviceAddress, channel, pliMessage);
}

void DirectChannelControlDialog::onSendPliT2HSBControl()
{
    uint8_t channel = m_pliT2HSBChannelSpinBox->value();
    uint8_t pliClan = m_pliClanSpinBox->value();
    uint8_t transition = m_pliTransitionSpinBox->value();
    uint8_t brightness = m_pliBrightnessSlider->value();
    uint8_t hue = m_pliHueSlider->value();
    uint8_t saturation = m_pliSaturationSlider->value();
    emit pliT2HSBControlRequested(m_deviceAddress, channel, pliClan, transition, brightness, hue, saturation);
}

void DirectChannelControlDialog::onChannelTypeChanged(int index)
{
    // This method is not used in this implementation but could be used for future enhancements
    Q_UNUSED(index)
}

void DirectChannelControlDialog::updatePliHexDisplay()
{
    // Update hex display when decimal changes
    if (sender() == m_pliMessageDecEdit) {
        uint32_t value = m_pliMessageDecEdit->text().toUInt();
        m_pliMessageHexEdit->setText(QString("%1").arg(value, 8, 16, QChar('0')).toUpper());
    }
    // Update decimal display when hex changes
    else if (sender() == m_pliMessageHexEdit) {
        bool ok;
        uint32_t value = m_pliMessageHexEdit->text().toUInt(&ok, 16);
        if (ok) {
            m_pliMessageDecEdit->setText(QString::number(value));
        }
    }
}
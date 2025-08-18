#include "zonelightingdialog.h"
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QLineEdit>
#include <QSlider>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QTimer>
#include <QCheckBox>
#include <QMessageBox>
#include <QDebug>

ZoneLightingDialog::ZoneLightingDialog(uint8_t deviceAddress, const QString& deviceName, QWidget *parent)
    : QDialog(parent)
    , m_deviceAddress(deviceAddress)
    , m_deviceName(deviceName)
    , m_tabWidget(nullptr)
    , m_buttonBox(nullptr)
    , m_singleZoneTab(nullptr)
    , m_zoneIdSpinBox(nullptr)
    , m_zoneNameEdit(nullptr)
    , m_redSlider(nullptr)
    , m_greenSlider(nullptr)
    , m_blueSlider(nullptr)
    , m_redValueLabel(nullptr)
    , m_greenValueLabel(nullptr)
    , m_blueValueLabel(nullptr)
    , m_colorPreviewLabel(nullptr)
    , m_colorTempSpinBox(nullptr)
    , m_intensitySlider(nullptr)
    , m_intensityValueLabel(nullptr)
    , m_programIdSpinBox(nullptr)
    , m_programColorSeqIndexSpinBox(nullptr)
    , m_programIntensitySlider(nullptr)
    , m_programRateSlider(nullptr)
    , m_programColorSequenceSlider(nullptr)
    , m_programIntensityValueLabel(nullptr)
    , m_programRateValueLabel(nullptr)
    , m_programColorSequenceValueLabel(nullptr)
    , m_zoneEnabledCheckBox(nullptr)
    , m_sendSingleZoneButton(nullptr)
    , m_multiZoneTab(nullptr)
    , m_startZoneSpinBox(nullptr)
    , m_endZoneSpinBox(nullptr)
    , m_delaySpinBox(nullptr)
    , m_waitForAckCheckBox(nullptr)
    , m_retryOnTimeoutCheckBox(nullptr)
    , m_sendMultipleZonesButton(nullptr)
    , m_sendAllZonesOnButton(nullptr)
    , m_sendAllZonesOffButton(nullptr)
    , m_sequenceTimer(nullptr)
    , m_currentZoneInSequence(0)
    , m_endZoneInSequence(0)
    , m_isOnSequence(true)
    , m_waitingForAcknowledgment(false)
    , m_acknowledgmentTimeoutTimer(nullptr)
    , m_currentRetryCount(0)
    , m_maxRetries(3)
{
    setupUI();
    setModal(false);
    setWindowTitle(QString("Zone Lighting Control - %1 (0x%2)")
                   .arg(m_deviceName)
                   .arg(m_deviceAddress, 2, 16, QChar('0')));
    resize(500, 600);
}

ZoneLightingDialog::~ZoneLightingDialog()
{
    // Qt will handle cleanup of child widgets
}

void ZoneLightingDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Create tab widget
    m_tabWidget = new QTabWidget();
    mainLayout->addWidget(m_tabWidget);
    
    // Setup tabs
    setupSingleZoneTab();
    setupMultiZoneTab();
    
    // Add dialog buttons
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(m_buttonBox);
    
    // Setup timer for multi-zone sequences
    m_sequenceTimer = new QTimer(this);
    m_sequenceTimer->setSingleShot(true);
    connect(m_sequenceTimer, &QTimer::timeout, this, &ZoneLightingDialog::sendNextZoneInSequence);
    
    // Setup acknowledgment timeout timer (5 second timeout)
    m_acknowledgmentTimeoutTimer = new QTimer(this);
    m_acknowledgmentTimeoutTimer->setSingleShot(true);
    m_acknowledgmentTimeoutTimer->setInterval(5000); // 5 seconds
    connect(m_acknowledgmentTimeoutTimer, &QTimer::timeout, this, &ZoneLightingDialog::onAcknowledgmentTimeout);
}

void ZoneLightingDialog::setupSingleZoneTab()
{
    m_singleZoneTab = new QWidget();
    m_tabWidget->addTab(m_singleZoneTab, "Single Zone Control");
    
    QVBoxLayout* layout = new QVBoxLayout(m_singleZoneTab);
    
    // Zone identification
    QGroupBox* zoneGroup = new QGroupBox("Zone Identification");
    QFormLayout* zoneLayout = new QFormLayout(zoneGroup);
    
    m_zoneIdSpinBox = new QSpinBox();
    m_zoneIdSpinBox->setRange(0, 252);
    m_zoneIdSpinBox->setValue(1);
    zoneLayout->addRow("Zone ID:", m_zoneIdSpinBox);
    
    m_zoneNameEdit = new QLineEdit();
    m_zoneNameEdit->setMaxLength(32);
    m_zoneNameEdit->setPlaceholderText("Zone name (max 32 chars)");
    zoneLayout->addRow("Zone Name:", m_zoneNameEdit);
    
    layout->addWidget(zoneGroup);
    
    // Color controls
    createColorControls(layout);
    
    // Program controls
    createProgramControls(layout);
    
    // Zone enabled
    QGroupBox* enabledGroup = new QGroupBox("Zone Status");
    QVBoxLayout* enabledLayout = new QVBoxLayout(enabledGroup);
    m_zoneEnabledCheckBox = new QCheckBox("Zone Enabled");
    m_zoneEnabledCheckBox->setChecked(true);
    enabledLayout->addWidget(m_zoneEnabledCheckBox);
    layout->addWidget(enabledGroup);
    
    // Send button
    m_sendSingleZoneButton = new QPushButton("Send Zone Control (PGN 130561)");
    connect(m_sendSingleZoneButton, &QPushButton::clicked, this, &ZoneLightingDialog::onSendSingleZone);
    layout->addWidget(m_sendSingleZoneButton);
    
    layout->addStretch();
}

void ZoneLightingDialog::createColorControls(QVBoxLayout* layout)
{
    QGroupBox* colorGroup = new QGroupBox("Color Control");
    QVBoxLayout* colorLayout = new QVBoxLayout(colorGroup);
    
    // RGB sliders
    QFormLayout* rgbLayout = new QFormLayout();
    
    // Red
    QHBoxLayout* redLayout = new QHBoxLayout();
    m_redSlider = new QSlider(Qt::Horizontal);
    m_redSlider->setRange(0, 255);
    m_redSlider->setValue(255);
    m_redValueLabel = new QLabel("255");
    m_redValueLabel->setMinimumWidth(30);
    redLayout->addWidget(m_redSlider);
    redLayout->addWidget(m_redValueLabel);
    rgbLayout->addRow("Red:", redLayout);
    
    // Green
    QHBoxLayout* greenLayout = new QHBoxLayout();
    m_greenSlider = new QSlider(Qt::Horizontal);
    m_greenSlider->setRange(0, 255);
    m_greenSlider->setValue(255);
    m_greenValueLabel = new QLabel("255");
    m_greenValueLabel->setMinimumWidth(30);
    greenLayout->addWidget(m_greenSlider);
    greenLayout->addWidget(m_greenValueLabel);
    rgbLayout->addRow("Green:", greenLayout);
    
    // Blue
    QHBoxLayout* blueLayout = new QHBoxLayout();
    m_blueSlider = new QSlider(Qt::Horizontal);
    m_blueSlider->setRange(0, 255);
    m_blueSlider->setValue(255);
    m_blueValueLabel = new QLabel("255");
    m_blueValueLabel->setMinimumWidth(30);
    blueLayout->addWidget(m_blueSlider);
    blueLayout->addWidget(m_blueValueLabel);
    rgbLayout->addRow("Blue:", blueLayout);
    
    colorLayout->addLayout(rgbLayout);
    
    // Color preview
    m_colorPreviewLabel = new QLabel();
    m_colorPreviewLabel->setMinimumHeight(40);
    m_colorPreviewLabel->setStyleSheet("border: 1px solid gray; background-color: rgb(255,255,255);");
    m_colorPreviewLabel->setText("Color Preview");
    m_colorPreviewLabel->setAlignment(Qt::AlignCenter);
    colorLayout->addWidget(m_colorPreviewLabel);
    
    // Color temperature
    m_colorTempSpinBox = new QSpinBox();
    m_colorTempSpinBox->setRange(0, 65532);
    m_colorTempSpinBox->setValue(3000);
    m_colorTempSpinBox->setSuffix(" K");
    colorLayout->addWidget(new QLabel("Color Temperature:"));
    colorLayout->addWidget(m_colorTempSpinBox);
    
    // Intensity
    QHBoxLayout* intensityLayout = new QHBoxLayout();
    m_intensitySlider = new QSlider(Qt::Horizontal);
    m_intensitySlider->setRange(0, 200);
    m_intensitySlider->setValue(200);
    m_intensityValueLabel = new QLabel("100.0%");
    m_intensityValueLabel->setMinimumWidth(50);
    intensityLayout->addWidget(m_intensitySlider);
    intensityLayout->addWidget(m_intensityValueLabel);
    colorLayout->addWidget(new QLabel("Intensity:"));
    colorLayout->addLayout(intensityLayout);
    
    layout->addWidget(colorGroup);
    
    // Connect sliders to update labels and preview
    connect(m_redSlider, &QSlider::valueChanged, this, &ZoneLightingDialog::onColorSliderChanged);
    connect(m_greenSlider, &QSlider::valueChanged, this, &ZoneLightingDialog::onColorSliderChanged);
    connect(m_blueSlider, &QSlider::valueChanged, this, &ZoneLightingDialog::onColorSliderChanged);
    connect(m_intensitySlider, &QSlider::valueChanged, this, &ZoneLightingDialog::onColorSliderChanged);
    
    // Initialize color preview
    updateColorPreview();
}

void ZoneLightingDialog::createProgramControls(QVBoxLayout* layout)
{
    QGroupBox* programGroup = new QGroupBox("Program Control");
    QFormLayout* programLayout = new QFormLayout(programGroup);
    
    m_programIdSpinBox = new QSpinBox();
    m_programIdSpinBox->setRange(0, 252);
    m_programIdSpinBox->setValue(0);
    programLayout->addRow("Program ID:", m_programIdSpinBox);
    
    m_programColorSeqIndexSpinBox = new QSpinBox();
    m_programColorSeqIndexSpinBox->setRange(0, 252);
    m_programColorSeqIndexSpinBox->setValue(0);
    programLayout->addRow("Color Sequence Index:", m_programColorSeqIndexSpinBox);
    
    // Program intensity
    QHBoxLayout* progIntensityLayout = new QHBoxLayout();
    m_programIntensitySlider = new QSlider(Qt::Horizontal);
    m_programIntensitySlider->setRange(0, 200);
    m_programIntensitySlider->setValue(100);
    m_programIntensityValueLabel = new QLabel("50.0%");
    m_programIntensityValueLabel->setMinimumWidth(50);
    progIntensityLayout->addWidget(m_programIntensitySlider);
    progIntensityLayout->addWidget(m_programIntensityValueLabel);
    programLayout->addRow("Program Intensity:", progIntensityLayout);
    
    // Program rate
    QHBoxLayout* progRateLayout = new QHBoxLayout();
    m_programRateSlider = new QSlider(Qt::Horizontal);
    m_programRateSlider->setRange(0, 200);
    m_programRateSlider->setValue(100);
    m_programRateValueLabel = new QLabel("50.0%");
    m_programRateValueLabel->setMinimumWidth(50);
    progRateLayout->addWidget(m_programRateSlider);
    progRateLayout->addWidget(m_programRateValueLabel);
    programLayout->addRow("Program Rate:", progRateLayout);
    
    // Program color sequence
    QHBoxLayout* progColorSeqLayout = new QHBoxLayout();
    m_programColorSequenceSlider = new QSlider(Qt::Horizontal);
    m_programColorSequenceSlider->setRange(0, 200);
    m_programColorSequenceSlider->setValue(100);
    m_programColorSequenceValueLabel = new QLabel("50.0%");
    m_programColorSequenceValueLabel->setMinimumWidth(50);
    progColorSeqLayout->addWidget(m_programColorSequenceSlider);
    progColorSeqLayout->addWidget(m_programColorSequenceValueLabel);
    programLayout->addRow("Program Color Sequence:", progColorSeqLayout);
    
    layout->addWidget(programGroup);
    
    // Connect program sliders
    connect(m_programIntensitySlider, &QSlider::valueChanged, [this](int value) {
        m_programIntensityValueLabel->setText(QString("%1%").arg(value * 0.5, 0, 'f', 1));
    });
    connect(m_programRateSlider, &QSlider::valueChanged, [this](int value) {
        m_programRateValueLabel->setText(QString("%1%").arg(value * 0.5, 0, 'f', 1));
    });
    connect(m_programColorSequenceSlider, &QSlider::valueChanged, [this](int value) {
        m_programColorSequenceValueLabel->setText(QString("%1%").arg(value * 0.5, 0, 'f', 1));
    });
}

void ZoneLightingDialog::setupMultiZoneTab()
{
    m_multiZoneTab = new QWidget();
    m_tabWidget->addTab(m_multiZoneTab, "Multi-Zone Control");
    
    QVBoxLayout* layout = new QVBoxLayout(m_multiZoneTab);
    
    // Zone range selection
    QGroupBox* rangeGroup = new QGroupBox("Zone Range");
    QFormLayout* rangeLayout = new QFormLayout(rangeGroup);
    
    m_startZoneSpinBox = new QSpinBox();
    m_startZoneSpinBox->setRange(0, 252);
    m_startZoneSpinBox->setValue(1);
    rangeLayout->addRow("Start Zone:", m_startZoneSpinBox);
    
    m_endZoneSpinBox = new QSpinBox();
    m_endZoneSpinBox->setRange(0, 252);
    m_endZoneSpinBox->setValue(12);
    rangeLayout->addRow("End Zone:", m_endZoneSpinBox);
    
    m_delaySpinBox = new QSpinBox();
    m_delaySpinBox->setRange(0, 5000);
    m_delaySpinBox->setValue(100);
    m_delaySpinBox->setSuffix(" ms");
    rangeLayout->addRow("Delay between zones:", m_delaySpinBox);
    
    m_waitForAckCheckBox = new QCheckBox("Wait for acknowledgment before next command");
    m_waitForAckCheckBox->setChecked(true); // Default to waiting for acknowledgments
    m_waitForAckCheckBox->setToolTip("When checked, waits for device acknowledgment before sending next command.\nWhen unchecked, sends commands at the specified delay interval.");
    rangeLayout->addRow("", m_waitForAckCheckBox);
    
    m_retryOnTimeoutCheckBox = new QCheckBox("Retry command on timeout (up to 3 times)");
    m_retryOnTimeoutCheckBox->setChecked(true); // Default to retrying on timeout
    m_retryOnTimeoutCheckBox->setToolTip("When checked, retries the command up to 3 times if no acknowledgment is received.\nOnly applies when waiting for acknowledgments is enabled.");
    rangeLayout->addRow("", m_retryOnTimeoutCheckBox);
    
    layout->addWidget(rangeGroup);
    
    // Multi-zone actions
    QGroupBox* actionsGroup = new QGroupBox("Multi-Zone Actions");
    QVBoxLayout* actionsLayout = new QVBoxLayout(actionsGroup);
    
    m_sendMultipleZonesButton = new QPushButton("Send Current Settings to Zone Range (Immediate)");
    connect(m_sendMultipleZonesButton, &QPushButton::clicked, this, &ZoneLightingDialog::onSendMultipleZones);
    actionsLayout->addWidget(m_sendMultipleZonesButton);
    
    actionsLayout->addWidget(new QLabel("Sequential Actions (respect acknowledgment setting):"));
    
    m_sendAllZonesOnButton = new QPushButton("Turn All Zones ON (Full Intensity)");
    connect(m_sendAllZonesOnButton, &QPushButton::clicked, this, &ZoneLightingDialog::onSendAllZonesOn);
    actionsLayout->addWidget(m_sendAllZonesOnButton);
    
    m_sendAllZonesOffButton = new QPushButton("Turn All Zones OFF");
    connect(m_sendAllZonesOffButton, &QPushButton::clicked, this, &ZoneLightingDialog::onSendAllZonesOff);
    actionsLayout->addWidget(m_sendAllZonesOffButton);
    
    layout->addWidget(actionsGroup);
    layout->addStretch();
}

void ZoneLightingDialog::onColorSliderChanged()
{
    // Update value labels
    m_redValueLabel->setText(QString::number(m_redSlider->value()));
    m_greenValueLabel->setText(QString::number(m_greenSlider->value()));
    m_blueValueLabel->setText(QString::number(m_blueSlider->value()));
    m_intensityValueLabel->setText(QString("%1%").arg(m_intensitySlider->value() * 0.5, 0, 'f', 1));
    
    // Update color preview
    updateColorPreview();
}

void ZoneLightingDialog::updateColorPreview()
{
    int red = m_redSlider->value();
    int green = m_greenSlider->value();
    int blue = m_blueSlider->value();
    
    QString colorStyle = QString("border: 1px solid gray; background-color: rgb(%1,%2,%3);")
                        .arg(red).arg(green).arg(blue);
    m_colorPreviewLabel->setStyleSheet(colorStyle);
    
    // Set text color based on brightness
    int brightness = (red + green + blue) / 3;
    QString textColor = (brightness > 128) ? "black" : "white";
    m_colorPreviewLabel->setStyleSheet(colorStyle + QString("color: %1;").arg(textColor));
}

void ZoneLightingDialog::onSendSingleZone()
{
    uint8_t zoneId = static_cast<uint8_t>(m_zoneIdSpinBox->value());
    QString zoneName = m_zoneNameEdit->text();
    uint8_t red = static_cast<uint8_t>(m_redSlider->value());
    uint8_t green = static_cast<uint8_t>(m_greenSlider->value());
    uint8_t blue = static_cast<uint8_t>(m_blueSlider->value());
    uint16_t colorTemp = static_cast<uint16_t>(m_colorTempSpinBox->value());
    uint8_t intensity = static_cast<uint8_t>(m_intensitySlider->value());
    uint8_t programId = static_cast<uint8_t>(m_programIdSpinBox->value());
    uint8_t programColorSeqIndex = static_cast<uint8_t>(m_programColorSeqIndexSpinBox->value());
    uint8_t programIntensity = static_cast<uint8_t>(m_programIntensitySlider->value());
    uint8_t programRate = static_cast<uint8_t>(m_programRateSlider->value());
    uint8_t programColorSequence = static_cast<uint8_t>(m_programColorSequenceSlider->value());
    bool zoneEnabled = m_zoneEnabledCheckBox->isChecked();
    
    emit zonePGN130561Requested(m_deviceAddress, zoneId, zoneName, red, green, blue,
                               colorTemp, intensity, programId, programColorSeqIndex,
                               programIntensity, programRate, programColorSequence, zoneEnabled);
}

void ZoneLightingDialog::onSendMultipleZones()
{
    int startZone = m_startZoneSpinBox->value();
    int endZone = m_endZoneSpinBox->value();
    
    if (startZone > endZone) {
        QMessageBox::warning(this, "Invalid Range", "Start zone must be less than or equal to end zone.");
        return;
    }
    
    // Send current settings to each zone in range
    // Note: This sends all commands immediately without waiting for acknowledgments
    // For sequential sending with acknowledgment waiting, use the ON/OFF buttons instead
    for (int zone = startZone; zone <= endZone; ++zone) {
        QString zoneName = QString("Zone %1").arg(zone);
        uint8_t red = static_cast<uint8_t>(m_redSlider->value());
        uint8_t green = static_cast<uint8_t>(m_greenSlider->value());
        uint8_t blue = static_cast<uint8_t>(m_blueSlider->value());
        uint16_t colorTemp = static_cast<uint16_t>(m_colorTempSpinBox->value());
        uint8_t intensity = static_cast<uint8_t>(m_intensitySlider->value());
        uint8_t programId = static_cast<uint8_t>(m_programIdSpinBox->value());
        uint8_t programColorSeqIndex = static_cast<uint8_t>(m_programColorSeqIndexSpinBox->value());
        uint8_t programIntensity = static_cast<uint8_t>(m_programIntensitySlider->value());
        uint8_t programRate = static_cast<uint8_t>(m_programRateSlider->value());
        uint8_t programColorSequence = static_cast<uint8_t>(m_programColorSequenceSlider->value());
        bool zoneEnabled = m_zoneEnabledCheckBox->isChecked();
        
        emit zonePGN130561Requested(m_deviceAddress, static_cast<uint8_t>(zone), zoneName,
                                   red, green, blue, colorTemp, intensity, programId,
                                   programColorSeqIndex, programIntensity, programRate,
                                   programColorSequence, zoneEnabled);
    }
}

void ZoneLightingDialog::onSendAllZonesOn()
{
    m_currentZoneInSequence = m_startZoneSpinBox->value();
    m_endZoneInSequence = m_endZoneSpinBox->value();
    m_isOnSequence = true;
    sendNextZoneInSequence();
}

void ZoneLightingDialog::onSendAllZonesOff()
{
    m_currentZoneInSequence = m_startZoneSpinBox->value();
    m_endZoneInSequence = m_endZoneSpinBox->value();
    m_isOnSequence = false;
    sendNextZoneInSequence();
}

void ZoneLightingDialog::sendNextZoneInSequence()
{
    if (m_currentZoneInSequence > m_endZoneInSequence) {
        m_waitingForAcknowledgment = false;
        return; // Sequence complete
    }
    
    // Reset retry count for new command
    m_currentRetryCount = 0;
    
    // Check if we should wait for acknowledgment or proceed immediately
    bool shouldWaitForAck = m_waitForAckCheckBox->isChecked();
    
    if (shouldWaitForAck) {
        // Send the command and wait for acknowledgment before sending next
        m_waitingForAcknowledgment = true;
        m_acknowledgmentTimeoutTimer->start(); // Start 5-second timeout
    } else {
        // Don't wait for acknowledgment, proceed with delay timer
        m_waitingForAcknowledgment = false;
    }
    
    // Send the command
    sendCurrentZoneCommand();
    
    // Note: Don't increment m_currentZoneInSequence here - do it after ACK or final timeout
    
    // If not waiting for acknowledgment, move to next zone immediately
    if (!shouldWaitForAck) {
        ++m_currentZoneInSequence;
        if (m_currentZoneInSequence <= m_endZoneInSequence) {
            int delay = m_delaySpinBox->value();
            m_sequenceTimer->start(delay);
        }
    }
}

void ZoneLightingDialog::sendCurrentZoneCommand()
{
    QString zoneName = QString("Zone %1").arg(m_currentZoneInSequence);
    uint8_t red, green, blue, intensity;
    
    if (m_isOnSequence) {
        // ON: White at full intensity
        red = green = blue = 255;
        intensity = 200; // 100%
    } else {
        // OFF: Black at zero intensity
        red = green = blue = 0;
        intensity = 0;
    }
    
    emit zonePGN130561Requested(m_deviceAddress, static_cast<uint8_t>(m_currentZoneInSequence), zoneName,
                               red, green, blue, 3000, intensity, 0, 0, 0, 0, 0, m_isOnSequence);
}

void ZoneLightingDialog::onCommandAcknowledged(uint8_t deviceAddress, uint32_t pgn, bool success)
{
    // Only process acknowledgments for our device and PGN 130561
    if (deviceAddress != m_deviceAddress || pgn != 130561) {
        return;
    }
    
    if (m_waitingForAcknowledgment) {
        m_waitingForAcknowledgment = false;
        m_acknowledgmentTimeoutTimer->stop();
        
        if (success) {
            // ACK received, move to next zone and continue sequence
            ++m_currentZoneInSequence;
            if (m_currentZoneInSequence <= m_endZoneInSequence) {
                int delay = m_delaySpinBox->value();
                m_sequenceTimer->start(delay);
            }
        } else {
            // NACK received, stop the sequence
            QMessageBox::warning(this, "Command Failed", 
                QString("Zone %1 command was rejected by device. Sequence stopped.")
                .arg(m_currentZoneInSequence));
        }
    }
}

void ZoneLightingDialog::onAcknowledgmentTimeout()
{
    if (m_waitingForAcknowledgment) {
        bool shouldRetry = m_retryOnTimeoutCheckBox->isChecked();
        
        if (shouldRetry && m_currentRetryCount < m_maxRetries) {
            // Retry the command
            m_currentRetryCount++;
            qDebug() << "Zone" << (m_currentZoneInSequence - 1) << "timeout, retrying attempt" << m_currentRetryCount << "of" << m_maxRetries;
            
            // Restart the timeout timer and resend the command
            m_acknowledgmentTimeoutTimer->start();
            sendCurrentZoneCommand();
        } else {
            // No more retries or retry disabled, stop the sequence
            m_waitingForAcknowledgment = false;
            ++m_currentZoneInSequence; // Move past the failed zone
            
            QString message;
            if (shouldRetry) {
                message = QString("Zone %1 command failed after %2 retries. Sequence stopped.")
                         .arg(m_currentZoneInSequence - 1)
                         .arg(m_maxRetries);
            } else {
                message = QString("No acknowledgment received for Zone %1 within 5 seconds. Sequence stopped.")
                         .arg(m_currentZoneInSequence - 1);
            }
            QMessageBox::warning(this, "Command Timeout", message);
        }
    }
}

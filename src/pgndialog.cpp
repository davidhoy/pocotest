#include "pgndialog.h"
#include "toastmanager.h"

#ifdef WASM_BUILD
#include "NMEA2000_WASM.h"
#else
#include "NMEA2000_SocketCAN.h"
#endif

#include <QMessageBox>
#include <QApplication>
#include <QHeaderView>
#include <QSplitter>
#include <QRegularExpression>
#include <QCheckBox>
#include <QLabel>
#include <QScreen>
#include <QScrollArea>
#include <QTimer>

extern tNMEA2000* nmea2000;

PGNDialog::PGNDialog(QWidget *parent)
    : QDialog(parent)
    , m_pgnComboBox(nullptr)
    , m_prioritySpinBox(nullptr)
    , m_sourceSpinBox(nullptr)
    , m_destinationSpinBox(nullptr)
    , m_dataTextEdit(nullptr)
    , m_sendButton(nullptr)
    , m_clearButton(nullptr)
    , m_cancelButton(nullptr)
    , m_parameterWidget(nullptr)
    , m_parameterLayout(nullptr)
    , m_intendedDestination(255)  // Default to broadcast
{
    setupUI();
    populateCommonPGNs();
    
    setWindowTitle("Send NMEA2000 PGN");
    setModal(true);
    
    // Set initial size - let dialog size itself first
    resize(600, 500);
}

void PGNDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Create a splitter for resizable sections
    QSplitter* splitter = new QSplitter(Qt::Vertical, this);
    
    // Top section - PGN Selection and Parameters
    QWidget* topWidget = new QWidget();
    QVBoxLayout* topLayout = new QVBoxLayout(topWidget);
    
    // PGN Selection Group
    QGroupBox* pgnGroup = new QGroupBox("PGN Selection");
    QFormLayout* pgnLayout = new QFormLayout(pgnGroup);
    
    m_pgnComboBox = new QComboBox();
    m_pgnComboBox->setEditable(true);
    connect(m_pgnComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PGNDialog::onPGNSelectionChanged);
    pgnLayout->addRow("PGN:", m_pgnComboBox);
    
    topLayout->addWidget(pgnGroup);
    
    // Message Parameters Group
    QGroupBox* msgGroup = new QGroupBox("Message Parameters");
    QFormLayout* msgLayout = new QFormLayout(msgGroup);
    
    m_prioritySpinBox = new QSpinBox();
    m_prioritySpinBox->setRange(0, 7);
    m_prioritySpinBox->setValue(6);
    msgLayout->addRow("Priority:", m_prioritySpinBox);
    
    m_sourceSpinBox = new QSpinBox();
    m_sourceSpinBox->setRange(0, 251);
    m_sourceSpinBox->setValue(22);  // Default source address
    msgLayout->addRow("Source:", m_sourceSpinBox);
    
    m_destinationSpinBox = new QSpinBox();
    m_destinationSpinBox->setRange(0, 255);
    m_destinationSpinBox->setValue(255);  // Broadcast
    msgLayout->addRow("Destination:", m_destinationSpinBox);
    
    topLayout->addWidget(msgGroup);
    
    // PGN-specific Parameters Group
    QGroupBox* paramGroup = new QGroupBox("PGN-specific Parameters");
    
    // Create scroll area for parameters to handle large parameter lists
    QScrollArea* paramScrollArea = new QScrollArea();
    paramScrollArea->setWidgetResizable(true);
    paramScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    paramScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    paramScrollArea->setMinimumHeight(150);  // Increased minimum height for better visibility
    paramScrollArea->setMaximumHeight(500);  // Increased maximum height to allow more parameters
    paramScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    m_parameterLayout = new QFormLayout();
    m_parameterLayout->setContentsMargins(5, 5, 5, 5);  // Reduce margins
    m_parameterLayout->setVerticalSpacing(4);            // Reduce spacing
    m_parameterWidget = new QWidget();
    m_parameterWidget->setLayout(m_parameterLayout);
    m_parameterWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    
    paramScrollArea->setWidget(m_parameterWidget);
    
    QVBoxLayout* paramGroupLayout = new QVBoxLayout(paramGroup);
    paramGroupLayout->setContentsMargins(5, 5, 5, 5);   // Reduce group margins
    paramGroupLayout->addWidget(paramScrollArea);
    
    topLayout->addWidget(paramGroup);
    
    splitter->addWidget(topWidget);
    
    // Bottom section - Data Input and Preview
    QWidget* bottomWidget = new QWidget();
    QVBoxLayout* bottomLayout = new QVBoxLayout(bottomWidget);
    
    // Raw Data Input Group (Collapsible)
    m_dataGroup = new QGroupBox();
    m_dataGroup->setFlat(true);
    QVBoxLayout* dataGroupLayout = new QVBoxLayout(m_dataGroup);
    dataGroupLayout->setContentsMargins(0, 0, 0, 0);
    
    // Create toggle button for raw data section
    m_dataToggleButton = new QPushButton("▶ Raw Data (Hex)");
    m_dataToggleButton->setFlat(true);
    m_dataToggleButton->setStyleSheet("QPushButton { text-align: left; padding: 5px; }");
    connect(m_dataToggleButton, &QPushButton::clicked, this, &PGNDialog::toggleDataSection);
    dataGroupLayout->addWidget(m_dataToggleButton);
    
    // Raw data content
    m_dataContent = new QWidget();
    QVBoxLayout* dataLayout = new QVBoxLayout(m_dataContent);
    dataLayout->setContentsMargins(10, 0, 10, 10);
    
    m_dataTextEdit = new QTextEdit();
    m_dataTextEdit->setMaximumHeight(80);
    m_dataTextEdit->setPlaceholderText("Enter hex data separated by spaces (e.g., FF 00 A1 B2)");
    dataLayout->addWidget(m_dataTextEdit);
    
    // Start collapsed
    m_dataContent->hide();
    
    dataGroupLayout->addWidget(m_dataContent);
    bottomLayout->addWidget(m_dataGroup);
    
    splitter->addWidget(bottomWidget);
    splitter->setStretchFactor(0, 3);  // Give 3x more space to parameters section
    splitter->setStretchFactor(1, 1);  // Bottom section gets minimal space when collapsed
    
    mainLayout->addWidget(splitter);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    m_sendButton = new QPushButton("Send PGN");
    m_clearButton = new QPushButton("Clear");
    m_cancelButton = new QPushButton("Cancel");
    
    connect(m_sendButton, &QPushButton::clicked, this, &PGNDialog::onSendPGN);
    connect(m_clearButton, &QPushButton::clicked, this, &PGNDialog::onClearData);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_clearButton);
    buttonLayout->addWidget(m_sendButton);
    buttonLayout->addWidget(m_cancelButton);
    
    mainLayout->addLayout(buttonLayout);
}

void PGNDialog::populateCommonPGNs()
{
    m_commonPGNs.clear();
    
    // Add some common PGNs
    m_commonPGNs.append({127245, "Rudder", "Rudder position", {"Rudder Position", "Rudder Offset"}});
    m_commonPGNs.append({127250, "Vessel Heading", "Vessel magnetic heading", {"SID", "Heading", "Deviation", "Variation", "Reference"}});
    m_commonPGNs.append({127251, "Rate of Turn", "Rate of turn", {"SID", "Rate of Turn"}});
    m_commonPGNs.append({127488, "Engine Parameters, Rapid", "Engine parameters rapid update", {"Engine Instance", "Engine Speed", "Engine Boost Pressure", "Engine Tilt/Trim"}});
    m_commonPGNs.append({127502, "Binary Switch Bank Control", "Control up to 28 binary switches (2 bits each: Off/On/Error/Unavailable)", {"Instance", "Switch 1", "Switch 2", "Switch 3", "Switch 4", "Switch 5", "Switch 6", "Switch 7", "Switch 8", "Switch 9", "Switch 10", "Switch 11", "Switch 12", "Switch 13", "Switch 14", "Switch 15", "Switch 16", "Switch 17", "Switch 18", "Switch 19", "Switch 20", "Switch 21", "Switch 22", "Switch 23", "Switch 24", "Switch 25", "Switch 26", "Switch 27", "Switch 28"}});
    m_commonPGNs.append({127505, "Fluid Level", "Fluid level", {"Instance", "Type", "Level", "Capacity"}});
    m_commonPGNs.append({127508, "Battery Status", "DC battery status", {"Battery Instance", "Voltage", "Current", "Temperature", "SID"}});
    m_commonPGNs.append({128259, "Boat Speed", "Speed through water", {"SID", "Speed Water Referenced", "Speed Water Referenced Type"}});
    m_commonPGNs.append({128267, "Water Depth", "Depth below transducer", {"SID", "Depth", "Offset", "Maximum Range Scale"}});
    m_commonPGNs.append({129025, "Position Rapid", "Latitude and longitude rapid update", {"Latitude", "Longitude"}});
    m_commonPGNs.append({129026, "COG & SOG Rapid", "Course and speed rapid update", {"SID", "COG Reference", "COG", "SOG"}});
    m_commonPGNs.append({129029, "GNSS Position", "GNSS position data", {"SID", "Date", "Time", "Latitude", "Longitude", "Altitude", "GNSS Type", "Method", "Integrity", "Number of SVs", "HDOP", "PDOP", "Geoidal Separation", "Reference Stations", "Reference Station Type", "Reference Station ID", "Age of DGNSS Corrections"}});
    m_commonPGNs.append({130306, "Wind Data", "Wind data", {"SID", "Wind Speed", "Wind Angle", "Reference"}});
    m_commonPGNs.append({130310, "Environmental Parameters", "Outside environmental conditions", {"SID", "Water Temperature", "Outside Ambient Air Temperature", "Atmospheric Pressure"}});
    m_commonPGNs.append({130312, "Temperature", "Temperature", {"SID", "Instance", "Source", "Actual Temperature", "Set Temperature"}});
    m_commonPGNs.append({130314, "Actual Pressure", "Pressure", {"SID", "Instance", "Source", "Actual Pressure"}});
    
    // Clear and populate the combo box
    m_pgnComboBox->clear();
    m_pgnComboBox->addItem("-- Select PGN --", 0);
    
    for (const auto& pgn : m_commonPGNs) {
        QString itemText = QString("%1 - %2").arg(pgn.pgn).arg(pgn.name);
        m_pgnComboBox->addItem(itemText, QVariant((qulonglong)pgn.pgn));
    }
    
    // Allow custom PGN entry
    m_pgnComboBox->addItem("Custom PGN...", -1);
}

void PGNDialog::onPGNSelectionChanged()
{
    QVariant data = m_pgnComboBox->currentData();
    if (!data.isValid()) return;
    
    unsigned long pgn = data.toULongLong();
    updateDataFieldsForPGN(pgn);
}

void PGNDialog::updateDataFieldsForPGN(unsigned long pgn)
{
    // Clear existing parameter widgets
    while (m_parameterLayout->count() > 0) {
        QLayoutItem* item = m_parameterLayout->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    
    if (pgn == 0 || pgn == (unsigned long)-1) {
        return;
    }
    
    // Find PGN info
    PGNInfo* pgnInfo = nullptr;
    for (auto& pgn_item : m_commonPGNs) {
        if (pgn_item.pgn == pgn) {
            pgnInfo = &pgn_item;
            break;
        }
    }
    
    if (pgnInfo) {
        // Add parameter fields based on PGN
        if (pgn == 127502) {
            // Special handling for Binary Switch Bank Control
            QLineEdit* instanceEdit = new QLineEdit();
            instanceEdit->setPlaceholderText("0-255");
            instanceEdit->setText("0");
            connect(instanceEdit, &QLineEdit::textChanged, this, &PGNDialog::onParameterChanged);
            m_parameterLayout->addRow("Instance:", instanceEdit);
            
            // Add checkboxes for switches 1-28 (as per NMEA2000 spec)
            // Group them in rows of 4 for better layout
            for (int i = 1; i <= 28; i++) {
                QCheckBox* switchBox = new QCheckBox();
                switchBox->setText(QString("Switch %1").arg(i));
                switchBox->setObjectName(QString("switch_%1").arg(i));
                switchBox->setToolTip(QString("Switch %1: Unchecked=Off, Checked=On").arg(i));
                connect(switchBox, &QCheckBox::toggled, this, &PGNDialog::onParameterChanged);
                m_parameterLayout->addRow(switchBox);
            }
        } else {
            // Standard parameter handling for other PGNs
            for (const QString& param : pgnInfo->parameters) {
                QLineEdit* paramEdit = new QLineEdit();
                paramEdit->setPlaceholderText("Enter value");
                connect(paramEdit, &QLineEdit::textChanged, this, &PGNDialog::onParameterChanged);
                m_parameterLayout->addRow(param + ":", paramEdit);
            }
        }
    } else {
        // Custom PGN - just show a note
        QLabel* note = new QLabel("For custom PGNs, use the Raw Data field below.");
        note->setStyleSheet("color: #666; font-style: italic;");
        m_parameterLayout->addRow(note);
    }
    
    // Update raw data to reflect initial parameter values
    updateRawDataFromParameters();
}

void PGNDialog::onSendPGN()
{
    if (!nmea2000) {
        ToastManager::instance()->showError("NMEA2000 interface not initialized!", this);
        return;
    }
    
    try {
        tN2kMsg msg = createMessageFromInputs();
        
        if (msg.PGN == 0) {
            ToastManager::instance()->showError("Invalid PGN specified!", this);
            return;
        }
        
        // Create a copy for signal emission to preserve original destination
        tN2kMsg msgCopy = msg;
        
        // Send the message
        bool success = nmea2000->SendMsg(msg);
        
        if (success) {
            // Emit signal to notify parent about the transmission
            emit messageTransmitted(msgCopy);
            
            // Show success toast notification
            ToastManager::instance()->showSuccess(
                QString("PGN %1 sent successfully! (%2 bytes)")
                .arg(msgCopy.PGN).arg(msgCopy.DataLen), this);
            // Keep dialog open for multiple sends - user can close manually
        } else {
            ToastManager::instance()->showError(
                QString("Failed to send PGN %1! Check NMEA2000 interface connection.")
                .arg(msg.PGN), this);
        }
    } catch (const std::exception& e) {
        ToastManager::instance()->showError(QString("Exception occurred: %1").arg(e.what()), this);
    } catch (...) {
        ToastManager::instance()->showError("Unknown error occurred while sending PGN!", this);
    }
}

void PGNDialog::onClearData()
{
    m_dataTextEdit->clear();
    m_prioritySpinBox->setValue(6);
    m_sourceSpinBox->setValue(22);
    m_destinationSpinBox->setValue(255);
    m_intendedDestination = 255;  // Also reset the intended destination
    m_pgnComboBox->setCurrentIndex(0);
    
    // Clear parameter fields
    while (m_parameterLayout->count() > 0) {
        QLayoutItem* item = m_parameterLayout->takeAt(0);
        if (item->widget()) {
            QLineEdit* lineEdit = qobject_cast<QLineEdit*>(item->widget());
            if (lineEdit) {
                lineEdit->clear();
            }
            QCheckBox* checkBox = qobject_cast<QCheckBox*>(item->widget());
            if (checkBox) {
                checkBox->setChecked(false);
            }
        }
    }
}

tN2kMsg PGNDialog::createMessageFromInputs()
{
    tN2kMsg msg;
    
    // Get PGN
    QVariant pgnData = m_pgnComboBox->currentData();
    unsigned long pgn = 0;
    
    if (pgnData.isValid() && pgnData.toULongLong() > 0) {
        pgn = pgnData.toULongLong();
    } else {
        // Try to parse from combo box text for custom PGN
        QString pgnText = m_pgnComboBox->currentText();
        bool ok;
        pgn = pgnText.toULong(&ok);
        if (!ok) {
            // Try to extract PGN from "PGN - Name" format
            QStringList parts = pgnText.split(" - ");
            if (parts.size() > 0) {
                pgn = parts[0].toULong(&ok);
            }
        }
    }
    
    if (pgn == 0) {
        return msg;  // Invalid PGN
    }
    
    // Set up basic message properties
    msg.SetPGN(pgn);
    msg.Priority = m_prioritySpinBox->value();
    msg.Source = m_sourceSpinBox->value();
    msg.Destination = m_intendedDestination;  // Use stored destination instead of spinbox
    
    // Add data from raw data field
    QString dataText = m_dataTextEdit->toPlainText().trimmed();
    if (!dataText.isEmpty()) {
        QStringList hexValues = dataText.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        
        for (const QString& hexValue : hexValues) {
            bool ok;
            unsigned char byte = hexValue.toUInt(&ok, 16);
            if (ok) {
                msg.AddByte(byte);
            }
        }
    } else {
        // If no raw data, try to create basic message based on PGN type
        // For now, just send minimal data
        switch (pgn) {
            case 127250: // Vessel Heading
                msg.AddByte(0xFF);  // SID
                msg.Add2ByteUDouble(0.0, 0.0001);  // Heading
                msg.Add2ByteDouble(0.0, 0.0001);   // Deviation
                msg.Add2ByteDouble(0.0, 0.0001);   // Variation
                msg.AddByte(0);  // Reference
                break;
            case 127502: // Binary Switch Bank Control
                {
                    // Get instance value from input fields
                    unsigned char instance = 0;
                    unsigned char switchData[7] = {0}; // 7 bytes for switch data (bytes 1-7)
                    
                    // Find the input fields
                    for (int i = 0; i < m_parameterLayout->rowCount(); i++) {
                        QLayoutItem* labelItem = m_parameterLayout->itemAt(i, QFormLayout::LabelRole);
                        QLayoutItem* fieldItem = m_parameterLayout->itemAt(i, QFormLayout::FieldRole);
                        QLayoutItem* spanningItem = m_parameterLayout->itemAt(i, QFormLayout::SpanningRole);
                        
                        if (labelItem && fieldItem) {
                            QLabel* label = qobject_cast<QLabel*>(labelItem->widget());
                            if (label) {
                                QString labelText = label->text();
                                if (labelText.startsWith("Instance:")) {
                                    QLineEdit* edit = qobject_cast<QLineEdit*>(fieldItem->widget());
                                    if (edit) {
                                        instance = edit->text().toUInt();
                                    }
                                }
                            }
                            
                            // Check for switch checkboxes in field role
                            QCheckBox* checkbox = qobject_cast<QCheckBox*>(fieldItem->widget());
                            if (checkbox) {
                                QString objectName = checkbox->objectName();
                                if (objectName.startsWith("switch_")) {
                                    int switchNum = objectName.split("_")[1].toInt();
                                    if (switchNum >= 1 && switchNum <= 28) {
                                        // Each switch uses 2 bits: 00=Off, 01=On, 10=Error, 11=Unavailable
                                        // Switch state: 1 if checked (On), 0 if unchecked (Off)
                                        unsigned char switchState = checkbox->isChecked() ? 1 : 0;
                                        
                                        // Calculate bit position in switch data bytes (switches start at bit 0 of byte 1)
                                        // Each switch uses 2 bits, so switch N is at bits (N-1)*2 and (N-1)*2+1
                                        int switchBitPos = (switchNum - 1) * 2;
                                        int byteIndex = switchBitPos / 8;
                                        int bitInByte = switchBitPos % 8;
                                        
                                        if (byteIndex >= 0 && byteIndex < 7) {
                                            switchData[byteIndex] |= (switchState << bitInByte);
                                        }
                                    }
                                }
                            }
                        }
                        
                        // Also check spanning role for checkboxes (addRow(widget) puts widgets there)
                        if (spanningItem) {
                            QCheckBox* checkbox = qobject_cast<QCheckBox*>(spanningItem->widget());
                            if (checkbox) {
                                QString objectName = checkbox->objectName();
                                if (objectName.startsWith("switch_")) {
                                    int switchNum = objectName.split("_")[1].toInt();
                                    if (switchNum >= 1 && switchNum <= 28) {
                                        // Each switch uses 2 bits: 00=Off, 01=On, 10=Error, 11=Unavailable
                                        // Switch state: 1 if checked (On), 0 if unchecked (Off)
                                        unsigned char switchState = checkbox->isChecked() ? 1 : 0;
                                        
                                        // Calculate bit position in switch data bytes (switches start at bit 0 of byte 1)
                                        // Each switch uses 2 bits, so switch N is at bits (N-1)*2 and (N-1)*2+1
                                        int switchBitPos = (switchNum - 1) * 2;
                                        int byteIndex = switchBitPos / 8;
                                        int bitInByte = switchBitPos % 8;
                                        
                                        if (byteIndex >= 0 && byteIndex < 7) {
                                            switchData[byteIndex] |= (switchState << bitInByte);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    
                    // PGN 127502 format (8 bytes total):
                    // Byte 0: Instance
                    // Bytes 1-7: Switch data (28 switches, 2 bits each)
                    msg.AddByte(instance);
                    for (int i = 0; i < 7; i++) {
                        msg.AddByte(switchData[i]);
                    }
                }
                break;
            default:
                // For unknown PGNs, add a single byte to make it valid
                msg.AddByte(0xFF);
                break;
        }
    }
    
    return msg;
}

void PGNDialog::setDestinationAddress(uint8_t address)
{
    m_intendedDestination = address;  // Store the intended destination
    if (m_destinationSpinBox) {
        m_destinationSpinBox->setValue(address);
    }
}

QSize PGNDialog::sizeHint() const
{
    // Provide a more conservative default size
    return QSize(550, 450);
}

void PGNDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    
    // Apply size constraints after the dialog is shown and has sized itself
    QTimer::singleShot(0, this, [this]() {
        QScreen* screen = QApplication::primaryScreen();
        if (!screen && !QApplication::screens().isEmpty()) {
            screen = QApplication::screens().first();
        }
        
        if (screen) {
            QRect screenGeometry = screen->availableGeometry();
            
            // Only apply constraints if dialog is larger than screen
            QSize dialogSize = size();
            int maxWidth = static_cast<int>(screenGeometry.width() * 0.9);
            int maxHeight = static_cast<int>(screenGeometry.height() * 0.9);
            
            bool needsResize = false;
            int newWidth = dialogSize.width();
            int newHeight = dialogSize.height();
            
            if (dialogSize.width() > maxWidth) {
                newWidth = maxWidth;
                needsResize = true;
            }
            
            if (dialogSize.height() > maxHeight) {
                newHeight = maxHeight;
                needsResize = true;
            }
            
            if (needsResize) {
                resize(newWidth, newHeight);
                
                // Center the dialog on screen
                int x = (screenGeometry.width() - newWidth) / 2 + screenGeometry.x();
                int y = (screenGeometry.height() - newHeight) / 2 + screenGeometry.y();
                
                // Ensure dialog is fully on screen
                x = qMax(screenGeometry.x(), qMin(x, screenGeometry.right() - newWidth));
                y = qMax(screenGeometry.y(), qMin(y, screenGeometry.bottom() - newHeight));
                
                move(x, y);
            }
        }
    });
}

void PGNDialog::toggleDataSection()
{
    if (m_dataContent->isVisible()) {
        m_dataContent->hide();
        m_dataToggleButton->setText("▶ Raw Data (Hex)");
    } else {
        m_dataContent->show();
        m_dataToggleButton->setText("▼ Raw Data (Hex)");
    }
    
    // Force layout update
    adjustSize();
}

void PGNDialog::onParameterChanged()
{
    // Update raw data whenever parameters change
    updateRawDataFromParameters();
}

void PGNDialog::updateRawDataFromParameters()
{
    QVariant pgnData = m_pgnComboBox->currentData();
    if (!pgnData.isValid()) return;
    
    unsigned long pgn = pgnData.toULongLong();
    if (pgn == 0) return;
    
    // Create a temporary message to generate hex data
    tN2kMsg tempMsg;
    tempMsg.SetPGN(pgn);
    tempMsg.Priority = m_prioritySpinBox->value();
    tempMsg.Source = m_sourceSpinBox->value();
    tempMsg.Destination = m_destinationSpinBox->value();
    
    // Generate data based on current parameter values
    switch (pgn) {
        case 127502: // Binary Switch Bank Control
        {
            // Get instance value
            unsigned char instance = 0;
            for (int i = 0; i < m_parameterLayout->rowCount(); ++i) {
                QLayoutItem* labelItem = m_parameterLayout->itemAt(i, QFormLayout::LabelRole);
                QLayoutItem* fieldItem = m_parameterLayout->itemAt(i, QFormLayout::FieldRole);
                
                if (labelItem && fieldItem) {
                    QLabel* label = qobject_cast<QLabel*>(labelItem->widget());
                    if (label && label->text() == "Instance:") {
                        QLineEdit* edit = qobject_cast<QLineEdit*>(fieldItem->widget());
                        if (edit) {
                            bool ok;
                            instance = edit->text().toUInt(&ok);
                            if (!ok) instance = 0;
                        }
                        break;
                    }
                }
            }
            
            // Build switch data bytes
            unsigned char switchData[7] = {0}; // 7 bytes for switch data (bytes 1-7)
            
            // Collect switch states from checkboxes
            for (int i = 0; i < m_parameterLayout->rowCount(); ++i) {
                QLayoutItem* fieldItem = m_parameterLayout->itemAt(i, QFormLayout::FieldRole);
                if (fieldItem) {
                    QCheckBox* checkBox = qobject_cast<QCheckBox*>(fieldItem->widget());
                    if (checkBox && checkBox->objectName().startsWith("switch_")) {
                        QString switchNumStr = checkBox->objectName().mid(7); // Remove "switch_" prefix
                        bool ok;
                        int switchNum = switchNumStr.toInt(&ok);
                        if (ok && switchNum >= 1 && switchNum <= 28) {
                            int byteIndex = (switchNum - 1) / 4;  // 4 switches per byte
                            int bitIndex = ((switchNum - 1) % 4) * 2;  // 2 bits per switch
                            
                            if (checkBox->isChecked()) {
                                switchData[byteIndex] |= (0x01 << bitIndex);  // Set switch to ON (01)
                            } else {
                                switchData[byteIndex] |= (0x00 << bitIndex);  // Set switch to OFF (00)
                            }
                        }
                    }
                }
            }
            
            // Add data to message
            tempMsg.AddByte(instance);
            for (int i = 0; i < 7; i++) {
                tempMsg.AddByte(switchData[i]);
            }
            break;
        }
        case 127250: // Vessel Heading
        {
            // Add basic vessel heading data with default values
            tempMsg.AddByte(0xFF);  // SID
            tempMsg.Add2ByteUDouble(0.0, 0.0001);  // Heading
            tempMsg.Add2ByteDouble(0.0, 0.0001);   // Deviation  
            tempMsg.Add2ByteDouble(0.0, 0.0001);   // Variation
            tempMsg.AddByte(0);  // Reference
            break;
        }
        default:
        {
            // For other PGNs, create basic data from parameters
            for (int i = 0; i < m_parameterLayout->rowCount(); ++i) {
                QLayoutItem* fieldItem = m_parameterLayout->itemAt(i, QFormLayout::FieldRole);
                if (fieldItem) {
                    QLineEdit* edit = qobject_cast<QLineEdit*>(fieldItem->widget());
                    if (edit) {
                        QString value = edit->text();
                        bool ok;
                        unsigned char byteVal = value.toUInt(&ok);
                        if (ok) {
                            tempMsg.AddByte(byteVal);
                        } else {
                            tempMsg.AddByte(0); // Default value
                        }
                    }
                }
            }
            break;
        }
    }
    
    // Convert message data to hex string
    QString hexData;
    for (int i = 0; i < tempMsg.DataLen; i++) {
        if (i > 0) hexData += " ";
        hexData += QString("%1").arg(tempMsg.Data[i], 2, 16, QChar('0')).toUpper();
    }
    
    // Update the raw data field
    m_dataTextEdit->setPlainText(hexData);
}

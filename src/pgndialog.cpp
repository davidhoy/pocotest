#include "pgndialog.h"
#include "NMEA2000_SocketCAN.h"
#include <QMessageBox>
#include <QApplication>
#include <QHeaderView>
#include <QSplitter>
#include <QRegExp>
#include <QCheckBox>
#include <QLabel>

extern tNMEA2000_SocketCAN* nmea2000;

PGNDialog::PGNDialog(QWidget *parent)
    : QDialog(parent)
    , m_pgnComboBox(nullptr)
    , m_prioritySpinBox(nullptr)
    , m_sourceSpinBox(nullptr)
    , m_destinationSpinBox(nullptr)
    , m_dataTextEdit(nullptr)
    , m_previewTextEdit(nullptr)
    , m_sendButton(nullptr)
    , m_clearButton(nullptr)
    , m_cancelButton(nullptr)
    , m_parameterWidget(nullptr)
    , m_parameterLayout(nullptr)
{
    setupUI();
    populateCommonPGNs();
    
    setWindowTitle("Send NMEA2000 PGN");
    setModal(true);
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
    m_parameterLayout = new QFormLayout();
    m_parameterWidget = new QWidget();
    m_parameterWidget->setLayout(m_parameterLayout);
    
    QVBoxLayout* paramGroupLayout = new QVBoxLayout(paramGroup);
    paramGroupLayout->addWidget(m_parameterWidget);
    
    topLayout->addWidget(paramGroup);
    
    splitter->addWidget(topWidget);
    
    // Bottom section - Data Input and Preview
    QWidget* bottomWidget = new QWidget();
    QVBoxLayout* bottomLayout = new QVBoxLayout(bottomWidget);
    
    // Raw Data Input Group
    QGroupBox* dataGroup = new QGroupBox("Raw Data (Hex)");
    QVBoxLayout* dataLayout = new QVBoxLayout(dataGroup);
    
    m_dataTextEdit = new QTextEdit();
    m_dataTextEdit->setMaximumHeight(80);
    m_dataTextEdit->setPlaceholderText("Enter hex data separated by spaces (e.g., FF 00 A1 B2)");
    dataLayout->addWidget(m_dataTextEdit);
    
    bottomLayout->addWidget(dataGroup);
    
    // Preview Group
    QGroupBox* previewGroup = new QGroupBox("Message Preview");
    QVBoxLayout* previewLayout = new QVBoxLayout(previewGroup);
    
    m_previewTextEdit = new QTextEdit();
    m_previewTextEdit->setReadOnly(true);
    m_previewTextEdit->setMaximumHeight(100);
    previewLayout->addWidget(m_previewTextEdit);
    
    bottomLayout->addWidget(previewGroup);
    
    splitter->addWidget(bottomWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);
    
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
    m_commonPGNs.append({127502, "Binary Switch Bank Control", "Control up to 8 binary switches (lights, pumps, etc.)", {"Instance", "Indicator Bank", "Switch 1", "Switch 2", "Switch 3", "Switch 4", "Switch 5", "Switch 6", "Switch 7", "Switch 8"}});
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
    
    // Update preview
    if (pgn > 0) {
        QString preview = QString("PGN: %1\nPriority: %2\nSource: %3\nDestination: %4")
                            .arg(pgn)
                            .arg(m_prioritySpinBox->value())
                            .arg(m_sourceSpinBox->value())
                            .arg(m_destinationSpinBox->value());
        m_previewTextEdit->setPlainText(preview);
    }
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
            m_parameterLayout->addRow("Instance:", instanceEdit);
            
            QLineEdit* indicatorEdit = new QLineEdit();
            indicatorEdit->setPlaceholderText("0-255");
            indicatorEdit->setText("0");
            m_parameterLayout->addRow("Indicator Bank:", indicatorEdit);
            
            // Add checkboxes for switches 1-8
            for (int i = 1; i <= 8; i++) {
                QCheckBox* switchBox = new QCheckBox();
                switchBox->setText(QString("Switch %1").arg(i));
                switchBox->setObjectName(QString("switch_%1").arg(i));
                switchBox->setToolTip(QString("Enable/disable switch %1 (bit %2)").arg(i).arg(i-1));
                m_parameterLayout->addRow(switchBox);
            }
        } else {
            // Standard parameter handling for other PGNs
            for (const QString& param : pgnInfo->parameters) {
                QLineEdit* paramEdit = new QLineEdit();
                paramEdit->setPlaceholderText("Enter value");
                m_parameterLayout->addRow(param + ":", paramEdit);
            }
        }
    } else {
        // Custom PGN - just show a note
        QLabel* note = new QLabel("For custom PGNs, use the Raw Data field below.");
        note->setStyleSheet("color: #666; font-style: italic;");
        m_parameterLayout->addRow(note);
    }
}

void PGNDialog::onSendPGN()
{
    if (!nmea2000) {
        QMessageBox::warning(this, "Error", "NMEA2000 interface not initialized!");
        return;
    }
    
    try {
        tN2kMsg msg = createMessageFromInputs();
        
        if (msg.PGN == 0) {
            QMessageBox::warning(this, "Error", "Invalid PGN specified!");
            return;
        }
        
        // Send the message
        bool success = nmea2000->SendMsg(msg);
        
        if (success) {
            QMessageBox::information(this, "Success", "PGN sent successfully!");
            accept();  // Close the dialog
        } else {
            QMessageBox::warning(this, "Error", "Failed to send PGN!");
        }
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Error", QString("Exception occurred: %1").arg(e.what()));
    } catch (...) {
        QMessageBox::critical(this, "Error", "Unknown error occurred while sending PGN!");
    }
}

void PGNDialog::onClearData()
{
    m_dataTextEdit->clear();
    m_previewTextEdit->clear();
    m_prioritySpinBox->setValue(6);
    m_sourceSpinBox->setValue(22);
    m_destinationSpinBox->setValue(255);
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
    msg.Destination = m_destinationSpinBox->value();
    
    // Add data from raw data field
    QString dataText = m_dataTextEdit->toPlainText().trimmed();
    if (!dataText.isEmpty()) {
        QStringList hexValues = dataText.split(QRegExp("\\s+"), Qt::SkipEmptyParts);
        
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
                    // Get instance and indicator values from input fields
                    unsigned char instance = 0;
                    unsigned char indicator = 0;
                    unsigned char switchByte = 0;
                    
                    // Find the input fields
                    for (int i = 0; i < m_parameterLayout->rowCount(); i++) {
                        QLayoutItem* labelItem = m_parameterLayout->itemAt(i, QFormLayout::LabelRole);
                        QLayoutItem* fieldItem = m_parameterLayout->itemAt(i, QFormLayout::FieldRole);
                        
                        if (labelItem && fieldItem) {
                            QLabel* label = qobject_cast<QLabel*>(labelItem->widget());
                            if (label) {
                                QString labelText = label->text();
                                if (labelText.startsWith("Instance:")) {
                                    QLineEdit* edit = qobject_cast<QLineEdit*>(fieldItem->widget());
                                    if (edit) {
                                        instance = edit->text().toUInt();
                                    }
                                } else if (labelText.startsWith("Indicator Bank:")) {
                                    QLineEdit* edit = qobject_cast<QLineEdit*>(fieldItem->widget());
                                    if (edit) {
                                        indicator = edit->text().toUInt();
                                    }
                                }
                            }
                            
                            // Check for switch checkboxes
                            QCheckBox* checkbox = qobject_cast<QCheckBox*>(fieldItem->widget());
                            if (checkbox) {
                                QString objectName = checkbox->objectName();
                                if (objectName.startsWith("switch_")) {
                                    int switchNum = objectName.split("_")[1].toInt();
                                    if (switchNum >= 1 && switchNum <= 8 && checkbox->isChecked()) {
                                        switchByte |= (1 << (switchNum - 1));
                                    }
                                }
                            }
                        }
                    }
                    
                    msg.AddByte(instance);
                    msg.AddByte(indicator);
                    msg.AddByte(switchByte);
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
    if (m_destinationSpinBox) {
        m_destinationSpinBox->setValue(address);
    }
}

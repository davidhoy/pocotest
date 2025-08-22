#include "pgnlogdialog.h"
#include <QMessageBox>
#include <QRegExp>
#include <QDebug>
#include <QFileDialog>
#include <QTextStream>
#include <QApplication>

PGNLogDialog::PGNLogDialog(QWidget *parent)
    : QDialog(parent)
    , m_logTable(nullptr)
    , m_clearButton(nullptr)
    , m_closeButton(nullptr)
    , m_saveButton(nullptr)
    , m_loadButton(nullptr)
    , m_clearFiltersButton(nullptr)
    , m_pauseButton(nullptr)
    , m_stopButton(nullptr)
    , m_statusLabel(nullptr)
    , m_filterGroup(nullptr)
    , m_sourceFilterEnabled(nullptr)
    , m_destinationFilterEnabled(nullptr)
    , m_sourceFilterCombo(nullptr)
    , m_destinationFilterCombo(nullptr)
    , m_filterLogicCombo(nullptr)
    , m_decodingEnabled(nullptr)
    , m_sourceFilter(255)        // No filter initially
    , m_destinationFilter(255)   // No filter initially
    , m_sourceFilterActive(false)
    , m_destinationFilterActive(false)
    , m_useAndLogic(true)        // Default to AND logic
    , m_logPaused(false)
    , m_logStopped(false)
    , m_showingLoadedLog(false)  // Initially showing live log
    , m_loadedLogFileName("")    // No loaded log initially
    , m_dbcDecoder(nullptr)      // Will be initialized in constructor body
{
    qDebug() << "PGNLogDialog constructor: Starting...";
    
    qDebug() << "PGNLogDialog constructor: About to call setupUI()";
    setupUI();
    qDebug() << "PGNLogDialog constructor: setupUI() completed";
    
    // Initialize the original DBC decoder - proven stable and fast
    qDebug() << "Initializing DBC Decoder...";
    
    m_dbcDecoder = new DBCDecoder(this);
    if (m_dbcDecoder && m_dbcDecoder->isInitialized()) {
        qDebug() << "DBC Decoder initialized successfully";
        qDebug() << m_dbcDecoder->getDecoderInfo();
    } else {
        qWarning() << "Failed to create or initialize DBC Decoder";
    }
    
    setWindowTitle("NMEA2000 PGN Message Log - LIVE");
    setModal(false);
    resize(900, 700);
    
    qDebug() << "PGNLogDialog constructor: Completed successfully";
}

PGNLogDialog::~PGNLogDialog()
{
}

void PGNLogDialog::setupUI()
{
    qDebug() << "setupUI: Starting...";
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    qDebug() << "setupUI: Created main layout";
    
    // Status label
    m_statusLabel = new QLabel("Live NMEA2000 PGN message log - Real-time updates");
    m_statusLabel->setStyleSheet("font-weight: bold; color: #333; padding: 5px;");
    mainLayout->addWidget(m_statusLabel);
    
    qDebug() << "setupUI: Created status label";
    
    // Filter group box
    m_filterGroup = new QGroupBox("Message Filters");
    QVBoxLayout* filterLayout = new QVBoxLayout(m_filterGroup);
    
    qDebug() << "setupUI: Created filter group";
    
    // Source filter row
    QHBoxLayout* sourceLayout = new QHBoxLayout();
    m_sourceFilterEnabled = new QCheckBox("Filter by Source:");
    m_sourceFilterCombo = new QComboBox();
    m_sourceFilterCombo->setEnabled(false);
    m_sourceFilterCombo->setMinimumWidth(200);
    sourceLayout->addWidget(m_sourceFilterEnabled);
    sourceLayout->addWidget(m_sourceFilterCombo);
    sourceLayout->addStretch();
    filterLayout->addLayout(sourceLayout);
    
    qDebug() << "setupUI: Created source filter";
    
    // Destination filter row
    QHBoxLayout* destLayout = new QHBoxLayout();
    m_destinationFilterEnabled = new QCheckBox("Filter by Destination:");
    m_destinationFilterCombo = new QComboBox();
    m_destinationFilterCombo->setEnabled(false);
    m_destinationFilterCombo->setMinimumWidth(200);
    destLayout->addWidget(m_destinationFilterEnabled);
    destLayout->addWidget(m_destinationFilterCombo);
    destLayout->addStretch();
    filterLayout->addLayout(destLayout);
    
    qDebug() << "setupUI: Created destination filter";
    
    // Filter logic row
    QHBoxLayout* logicLayout = new QHBoxLayout();
    logicLayout->addWidget(new QLabel("Filter Logic:"));
    m_filterLogicCombo = new QComboBox();
    m_filterLogicCombo->addItem("AND (both conditions must match)");
    m_filterLogicCombo->addItem("OR (either condition can match)");
    m_filterLogicCombo->setCurrentIndex(0); // Default to AND
    m_filterLogicCombo->setMinimumWidth(250);
    logicLayout->addWidget(m_filterLogicCombo);
    logicLayout->addStretch();
    filterLayout->addLayout(logicLayout);
    
    // DBC Decoding option
    QHBoxLayout* decodingLayout = new QHBoxLayout();
    m_decodingEnabled = new QCheckBox("Enable DBC Decoding");
    m_decodingEnabled->setChecked(true); // Default to enabled
    m_decodingEnabled->setToolTip("Decode known NMEA2000 messages using DBC definitions");
    decodingLayout->addWidget(m_decodingEnabled);
    decodingLayout->addStretch();
    filterLayout->addLayout(decodingLayout);
    
    // Clear filters button
    QHBoxLayout* filterButtonLayout = new QHBoxLayout();
    m_clearFiltersButton = new QPushButton("Clear All Filters");
    filterButtonLayout->addWidget(m_clearFiltersButton);
    filterButtonLayout->addStretch();
    filterLayout->addLayout(filterButtonLayout);
    
    mainLayout->addWidget(m_filterGroup);
    
    // Initialize combo boxes with common values
    QStringList commonAddresses;
    commonAddresses << "Any" << "Broadcast (255)";
    for (int i = 0; i < 254; i++) {
        commonAddresses << QString("0x%1 (%2)").arg(i, 2, 16, QChar('0')).toUpper().arg(i);
    }
    
    m_sourceFilterCombo->addItems(commonAddresses);
    m_destinationFilterCombo->addItems(commonAddresses);
    
    // Connect filter signals
    connect(m_sourceFilterEnabled, &QCheckBox::toggled, this, &PGNLogDialog::onSourceFilterEnabled);
    connect(m_destinationFilterEnabled, &QCheckBox::toggled, this, &PGNLogDialog::onDestinationFilterEnabled);
    connect(m_sourceFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &PGNLogDialog::onSourceFilterChanged);
    connect(m_destinationFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &PGNLogDialog::onDestinationFilterChanged);
    connect(m_filterLogicCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PGNLogDialog::onFilterLogicChanged);
    connect(m_clearFiltersButton, &QPushButton::clicked, this, &PGNLogDialog::onClearFilters);
    connect(m_decodingEnabled, &QCheckBox::toggled, this, &PGNLogDialog::onToggleDecoding);
    
    // Timestamp mode control
    m_timestampModeCheck = new QCheckBox("Show relative timestamps (ms)");
    m_timestampModeCheck->setChecked(false); // Default to absolute
    connect(m_timestampModeCheck, &QCheckBox::toggled, this, [this](bool checked) {
        setTimestampMode(checked ? Relative : Absolute);
    });

    // Log table
    m_logTable = new QTableWidget();
    m_logTable->setColumnCount(9);
    QStringList headers;
    headers << "Timestamp" << "PGN" << "Message Name" << "Pri" << "Src" << "Dst" << "Len" << "Raw Data" << "Decoded";
    m_logTable->setHorizontalHeaderLabels(headers);
    
    // Configure table
    m_logTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_logTable->setAlternatingRowColors(true);
    m_logTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents); // Timestamp
    m_logTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents); // PGN
    m_logTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive); // Message Name
    m_logTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents); // Priority
    m_logTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents); // Source
    m_logTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents); // Destination
    m_logTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents); // Length
    m_logTable->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Interactive); // Raw Data - interactive sizing
    m_logTable->horizontalHeader()->setSectionResizeMode(8, QHeaderView::Stretch); // Decoded - stretch to fill
    m_logTable->verticalHeader()->setVisible(false);
    m_logTable->setSortingEnabled(false);
    
    // Set smaller font for the table
    QFont tableFont("Consolas, Monaco, monospace", 9);
    m_logTable->setFont(tableFont);
    
    // Connect table click signal
    connect(m_logTable, &QTableWidget::cellClicked, this, &PGNLogDialog::onTableItemClicked);
    
    mainLayout->addWidget(m_timestampModeCheck);
    mainLayout->addWidget(m_logTable);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    // Control buttons
    m_startButton = new QPushButton("Start");
    m_pauseButton = new QPushButton("Pause");
    m_stopButton = new QPushButton("Stop");
    m_clearButton = new QPushButton("Clear Log");
    m_saveButton = new QPushButton("Save Log...");
    m_loadButton = new QPushButton("Load Log...");
    m_closeButton = new QPushButton("Close");
    
    // Set button states
    m_startButton->setEnabled(false); // Start disabled initially
    m_pauseButton->setEnabled(true);  // Pause enabled initially
    m_stopButton->setEnabled(true);   // Stop enabled initially
    
    connect(m_startButton, &QPushButton::clicked, this, &PGNLogDialog::onStartClicked);
    connect(m_pauseButton, &QPushButton::clicked, this, &PGNLogDialog::onPauseClicked);
    connect(m_stopButton, &QPushButton::clicked, this, &PGNLogDialog::onStopClicked);
    connect(m_clearButton, &QPushButton::clicked, this, &PGNLogDialog::clearLog);
    connect(m_saveButton, &QPushButton::clicked, this, &PGNLogDialog::onSaveLogClicked);
    connect(m_loadButton, &QPushButton::clicked, this, &PGNLogDialog::onLoadLogClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &PGNLogDialog::onCloseClicked);
    
    buttonLayout->addWidget(m_startButton);
    buttonLayout->addWidget(m_pauseButton);
    buttonLayout->addWidget(m_stopButton);
    buttonLayout->addWidget(m_clearButton);
    buttonLayout->addWidget(m_saveButton);
    buttonLayout->addWidget(m_loadButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_closeButton);
    
    mainLayout->addLayout(buttonLayout);
}

void PGNLogDialog::appendMessage(const tN2kMsg& msg)
{
    // Check if logging is stopped or paused
    if (m_logStopped || m_logPaused) {
        return; // Don't add new messages
    }
    
    // Apply filters
    if (!messagePassesFilter(msg)) {
        return; // Skip this message
    }
    
    // Format hex data payload
    QString hexData = "";
    for (int i = 0; i < msg.DataLen; i++) {
        if (i > 0) hexData += " ";
        hexData += QString("%1").arg(msg.Data[i], 2, 16, QChar('0')).toUpper();
    }
    if (hexData.isEmpty()) {
        hexData = "(no data)";
    }

    // Get decoded message name and data using original decoder with enhancements
    // Original decoder works with PGN directly, not CAN ID
    
    QString messageName;
    if (m_dbcDecoder && m_dbcDecoder->canDecode(msg.PGN)) {
        messageName = m_dbcDecoder->getCleanMessageName(msg.PGN);  // Use enhanced formatting
    } else {
        messageName = QString("PGN %1").arg(msg.PGN);
    }
    
    // Prepare decoded data
    QString decodedData = "";
    if (m_decodingEnabled->isChecked() && m_dbcDecoder && m_dbcDecoder->canDecode(msg.PGN)) {
        decodedData = m_dbcDecoder->getFormattedDecoded(msg);
        if (decodedData.isEmpty() || decodedData == "Raw data" || decodedData.startsWith("PGN")) {
            decodedData = "(not decoded)";
        }
    } else {
        decodedData = "(decoding disabled)";
    }

    // Add new row to table
    int row = m_logTable->rowCount();
    m_logTable->insertRow(row);

    // Timestamp column (0)
    QDateTime now = QDateTime::currentDateTime();
    QString tsText;
    if (m_timestampMode == Absolute) {
        tsText = now.toString("HH:mm:ss.zzz");
    } else {
        qint64 deltaMs = 0;
        if (!m_messageTimestamps.isEmpty()) {
            deltaMs = m_messageTimestamps.last().msecsTo(now);
        }
        tsText = QString::number(deltaMs) + " ms";
    }
    m_messageTimestamps.append(now);
    QTableWidgetItem* tsItem = new QTableWidgetItem(tsText);
    tsItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_logTable->setItem(row, 0, tsItem);

    // Column 1: PGN
    QTableWidgetItem* pgnItem = new QTableWidgetItem(QString::number(msg.PGN));
    pgnItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_logTable->setItem(row, 1, pgnItem);

    // Column 2: Message Name
    QTableWidgetItem* nameItem = new QTableWidgetItem(messageName);
    nameItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_logTable->setItem(row, 2, nameItem);

    // Column 3: Priority
    QTableWidgetItem* priItem = new QTableWidgetItem(QString::number(msg.Priority));
    priItem->setTextAlignment(Qt::AlignCenter);
    m_logTable->setItem(row, 3, priItem);

    // Column 4: Source
    QTableWidgetItem* srcItem = new QTableWidgetItem(QString("%1").arg(msg.Source, 2, 16, QChar('0')).toUpper());
    srcItem->setTextAlignment(Qt::AlignCenter);
    m_logTable->setItem(row, 4, srcItem);

    // Column 5: Destination
    QString destText = QString("%1").arg(msg.Destination, 2, 16, QChar('0')).toUpper();
    QTableWidgetItem* destItem = new QTableWidgetItem(destText);
    destItem->setTextAlignment(Qt::AlignCenter);
    m_logTable->setItem(row, 5, destItem);

    // Column 6: Length
    QTableWidgetItem* lenItem = new QTableWidgetItem(QString::number(msg.DataLen));
    lenItem->setTextAlignment(Qt::AlignCenter);
    m_logTable->setItem(row, 6, lenItem);

    // Column 7: Raw Data
    QTableWidgetItem* rawDataItem = new QTableWidgetItem(hexData);
    rawDataItem->setFont(QFont("Consolas, Monaco, monospace", 9));
    m_logTable->setItem(row, 7, rawDataItem);

    // Column 8: Decoded Data
    QTableWidgetItem* decodedItem = new QTableWidgetItem(decodedData);
    decodedItem->setFont(QFont("Consolas, Monaco, monospace", 9));
    m_logTable->setItem(row, 8, decodedItem);

    // Auto-scroll to bottom
    m_logTable->scrollToBottom();
}

void PGNLogDialog::appendSentMessage(const tN2kMsg& msg)
{
    // Check if logging is stopped or paused
    if (m_logStopped || m_logPaused) {
        return; // Don't add new messages
    }
    
    // Apply filters (sent messages should also be filtered)
    if (!messagePassesFilter(msg)) {
        return; // Skip this message
    }

    // Format hex data payload
    QString hexData = "";
    for (int i = 0; i < msg.DataLen; i++) {
        if (i > 0) hexData += " ";
        hexData += QString("%1").arg(msg.Data[i], 2, 16, QChar('0')).toUpper();
    }
    if (hexData.isEmpty()) {
        hexData = "(no data)";
    }

    // Get decoded message name and data using original decoder with enhancements (sent messages)
    
    QString messageName;
    if (m_dbcDecoder && m_dbcDecoder->canDecode(msg.PGN)) {
        messageName = m_dbcDecoder->getCleanMessageName(msg.PGN);  // Use enhanced formatting
    } else {
        messageName = QString("PGN %1").arg(msg.PGN);
    }
    
    // Prepare decoded data for sent messages
    QString decodedData = "";
    if (m_decodingEnabled->isChecked() && m_dbcDecoder && m_dbcDecoder->canDecode(msg.PGN)) {
        decodedData = m_dbcDecoder->getFormattedDecoded(msg);
        if (decodedData.isEmpty() || decodedData == "Raw data" || decodedData.startsWith("PGN")) {
            decodedData = "(not decoded)";
        }
    } else {
        decodedData = "(decoding disabled)";
    }

    // Add new row to table
    int row = m_logTable->rowCount();
    m_logTable->insertRow(row);
    
    QColor blueColor(0, 0, 255);
    
    // Timestamp column (0)
    QDateTime now = QDateTime::currentDateTime();
    QString tsText;
    if (m_timestampMode == Absolute) {
        tsText = now.toString("HH:mm:ss.zzz");
    } else {
        qint64 deltaMs = 0;
        if (!m_messageTimestamps.isEmpty()) {
            deltaMs = m_messageTimestamps.last().msecsTo(now);
        }
        tsText = QString::number(deltaMs) + " ms";
    }
    m_messageTimestamps.append(now);
    QTableWidgetItem* tsItem = new QTableWidgetItem(tsText);
    tsItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    tsItem->setForeground(QBrush(blueColor));
    m_logTable->setItem(row, 0, tsItem);
    
    // Column 1: PGN (with "Sent:" prefix for sent messages)
    QTableWidgetItem* pgnItem = new QTableWidgetItem(QString("Sent: %1").arg(msg.PGN));
    pgnItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    pgnItem->setForeground(QBrush(blueColor));
    m_logTable->setItem(row, 1, pgnItem);
    
    // Column 2: Message Name
    QTableWidgetItem* nameItem = new QTableWidgetItem(messageName);
    nameItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    nameItem->setForeground(QBrush(blueColor));
    m_logTable->setItem(row, 2, nameItem);
    
    // Column 3: Priority
    QTableWidgetItem* priItem = new QTableWidgetItem(QString::number(msg.Priority));
    priItem->setTextAlignment(Qt::AlignCenter);
    priItem->setForeground(QBrush(blueColor));
    m_logTable->setItem(row, 3, priItem);
    
    // Column 4: Source
    QTableWidgetItem* srcItem = new QTableWidgetItem(QString("%1").arg(msg.Source, 2, 16, QChar('0')).toUpper());
    srcItem->setTextAlignment(Qt::AlignCenter);
    srcItem->setForeground(QBrush(blueColor));
    m_logTable->setItem(row, 4, srcItem);
    
    // Column 5: Destination
    QString destText = QString("%1").arg(msg.Destination, 2, 16, QChar('0')).toUpper();
    QTableWidgetItem* destItem = new QTableWidgetItem(destText);
    destItem->setTextAlignment(Qt::AlignCenter);
    destItem->setForeground(QBrush(blueColor));
    m_logTable->setItem(row, 5, destItem);
    
    // Column 6: Length
    QTableWidgetItem* lenItem = new QTableWidgetItem(QString::number(msg.DataLen));
    lenItem->setTextAlignment(Qt::AlignCenter);
    lenItem->setForeground(QBrush(blueColor));
    m_logTable->setItem(row, 6, lenItem);
    
    // Column 7: Raw Data
    QTableWidgetItem* rawDataItem = new QTableWidgetItem(hexData);
    rawDataItem->setFont(QFont("Consolas, Monaco, monospace", 9));
    rawDataItem->setForeground(QBrush(blueColor));
    m_logTable->setItem(row, 7, rawDataItem);
    
    // Column 8: Decoded Data
    QTableWidgetItem* decodedItem = new QTableWidgetItem(decodedData);
    decodedItem->setFont(QFont("Consolas, Monaco, monospace", 9));
    decodedItem->setForeground(QBrush(blueColor));
    m_logTable->setItem(row, 8, decodedItem);
    
    // Auto-scroll to bottom
    m_logTable->scrollToBottom();
}

void PGNLogDialog::clearLog()
{
    m_logTable->setRowCount(0);
    m_messageTimestamps.clear();
    
    // Reset to running state when clearing
    m_logPaused = false;
    m_logStopped = false;
    
    // Update button states
    m_startButton->setEnabled(false);
    m_pauseButton->setEnabled(true);
    m_stopButton->setEnabled(true);
    
    // Update status
    m_statusLabel->setText("Live NMEA2000 PGN message log - Real-time updates");
    m_statusLabel->setStyleSheet("font-weight: bold; color: #333; padding: 5px;");
    
    // Switch back to live mode when clearing
    m_showingLoadedLog = false;
    m_loadedLogFileName = "";
    updateWindowTitle();
}

void PGNLogDialog::clearLogForLoad()
{
    // Clear table and timestamps without changing logging state
    m_logTable->setRowCount(0);
    m_messageTimestamps.clear();
    
    // Keep logging stopped and buttons in their current state
    // Status will be updated after load completes
}

void PGNLogDialog::onCloseClicked()
{
    hide(); // Hide instead of close so it can be reopened
}

void PGNLogDialog::onSaveLogClicked()
{
    if (m_logTable->rowCount() == 0) {
        QMessageBox::information(this, "Save Log", "No messages to save. The log is empty.");
        return;
    }
    
    // Get current timestamp for default filename
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString defaultFileName = QString("nmea2000_log_%1.pgnlog").arg(timestamp);
    
    // Show file save dialog
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Save PGN Log",
        defaultFileName,
        "PGN Log Files (*.pgnlog);;Text Files (*.txt);;All Files (*)"
    );
    
    if (fileName.isEmpty()) {
        return; // User cancelled
    }
    
    // Write log to file
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Save Error", 
                             QString("Could not open file for writing:\n%1\n\nError: %2")
                             .arg(fileName)
                             .arg(file.errorString()));
        return;
    }
    
    QTextStream out(&file);
    
    // Write structured header
    out << "# NMEA2000 PGN Message Log\n";
    out << "# Generated by Lumitec Poco Tester\n";
    out << "# Format Version: 1.0\n";
    out << "# Export Time: " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n";
    out << "# Total Messages: " << m_logTable->rowCount() << "\n";
    
    // Write filter information if active
    if (m_sourceFilterActive || m_destinationFilterActive) {
        out << "# Active Filters:\n";
        if (m_sourceFilterActive) {
            out << "# Source Filter: 0x" << QString::number(m_sourceFilter, 16).toUpper() << "\n";
        }
        if (m_destinationFilterActive) {
            out << "# Destination Filter: 0x" << QString::number(m_destinationFilter, 16).toUpper() << "\n";
        }
        if (m_sourceFilterActive && m_destinationFilterActive) {
            out << "# Filter Logic: " << (m_useAndLogic ? "AND" : "OR") << "\n";
        }
    }
    
    out << "#\n";
    out << "# Format: TIMESTAMP | PGN | PRIORITY | SOURCE | DESTINATION | LENGTH | RAW_DATA\n";
    out << "# All values are preserved in original format for exact reconstruction\n";
    out << "#\n";
    
    // Write all log entries in structured format
    for (int row = 0; row < m_logTable->rowCount(); row++) {
        QStringList messageData;
        
        // Extract core message data for reconstruction
        QString timestamp = m_logTable->item(row, 0) ? m_logTable->item(row, 0)->text() : "";
        QString pgn = m_logTable->item(row, 1) ? m_logTable->item(row, 1)->text() : "";
        QString priority = m_logTable->item(row, 3) ? m_logTable->item(row, 3)->text() : "";
        QString source = m_logTable->item(row, 4) ? m_logTable->item(row, 4)->text() : "";
        QString destination = m_logTable->item(row, 5) ? m_logTable->item(row, 5)->text() : "";
        QString length = m_logTable->item(row, 6) ? m_logTable->item(row, 6)->text() : "";
        QString rawData = m_logTable->item(row, 7) ? m_logTable->item(row, 7)->text() : "";
        
        // Clean up PGN field (remove "Sent:" prefix if present)
        if (pgn.startsWith("Sent: ")) {
            pgn = pgn.mid(6);
        }
        
        // Format: TIMESTAMP | PGN | PRIORITY | SOURCE | DESTINATION | LENGTH | RAW_DATA
        messageData << timestamp << pgn << priority << source << destination << length << rawData;
        out << messageData.join(" | ") << "\n";
        
        // Append current decoded information as human-readable comments (without reserved fields)
        QString messageName = m_logTable->item(row, 2) ? m_logTable->item(row, 2)->text() : "";
        
        if (!messageName.isEmpty() && messageName != QString("PGN %1").arg(pgn)) {
            out << "#   Message: " << pgn << " - " << messageName << "\n";
        }
        
        // Reconstruct message for clean decoding without reserved fields
        if (m_dbcDecoder && m_decodingEnabled->isChecked()) {
            bool ok;
            uint32_t pgnNum = pgn.toUInt(&ok);
            if (ok && m_dbcDecoder->canDecode(pgnNum)) {
                // Parse raw data to reconstruct message
                QStringList hexBytes = rawData.split(" ", Qt::SkipEmptyParts);
                uint8_t dataLen = length.toUInt(&ok);
                if (ok && !rawData.isEmpty() && rawData != "(no data)") {
                    // Reconstruct N2K message
                    tN2kMsg reconstructedMsg;
                    reconstructedMsg.PGN = pgnNum;
                    reconstructedMsg.Priority = priority.toUInt(&ok) ? priority.toUInt() : 6;
                    reconstructedMsg.Source = source.toUInt(&ok, 16) ? source.toUInt(nullptr, 16) : 0;
                    reconstructedMsg.Destination = destination.toUInt(&ok, 16) ? destination.toUInt(nullptr, 16) : 255;
                    reconstructedMsg.DataLen = qMin(dataLen, (uint8_t)hexBytes.size());
                    
                    // Parse hex data into byte array
                    for (int i = 0; i < reconstructedMsg.DataLen && i < hexBytes.size(); i++) {
                        bool hexOk;
                        uint8_t byteVal = hexBytes[i].toUInt(&hexOk, 16);
                        if (hexOk) {
                            reconstructedMsg.Data[i] = byteVal;
                        } else {
                            reconstructedMsg.Data[i] = 0;
                        }
                    }
                    
                    // Get decoded data without reserved fields
                    QString cleanDecodedData = m_dbcDecoder->getFormattedDecodedForSave(reconstructedMsg);
                    if (!cleanDecodedData.isEmpty() && cleanDecodedData != "Raw data" && cleanDecodedData != "(not decoded)") {
                        // Split decoded data into multiple lines for readability if it's long
                        if (cleanDecodedData.length() > 80) {
                            QStringList decodedLines = cleanDecodedData.split(", ");
                            out << "#   Decoded: " << decodedLines.first() << "\n";
                            for (int i = 1; i < decodedLines.size(); i++) {
                                out << "#           " << decodedLines[i] << "\n";
                            }
                        } else {
                            out << "#   Decoded: " << cleanDecodedData << "\n";
                        }
                    }
                }
            }
        }
        
        // Add blank line for readability between messages
        out << "\n";
    }
    
    file.close();
    
    // Show success message
    QMessageBox::information(this, "Save Successful", 
                           QString("PGN log saved successfully!\n\nFile: %1\nMessages: %2\n\nThis file can be reloaded to display the PGN trace and re-decode the data.")
                           .arg(fileName)
                           .arg(m_logTable->rowCount()));
}

void PGNLogDialog::onLoadLogClicked()
{
    // Show file open dialog first
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Load PGN Log",
        "",
        "PGN Log Files (*.pgnlog);;Text Files (*.txt);;All Files (*)"
    );
    
    if (fileName.isEmpty()) {
        return; // User cancelled - no action needed
    }
    
    // Automatically stop live logging when loading a log file
    if (!m_logStopped) {
        onStopClicked();
    }
    
    // Read log from file
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Load Error", 
                             QString("Could not open file for reading:\n%1\n\nError: %2")
                             .arg(fileName)
                             .arg(file.errorString()));
        return;
    }
    
    QTextStream in(&file);
    
    // Clear current log without restarting live logging
    clearLogForLoad();
    
    int loadedMessages = 0;
    int skippedMessages = 0;
    
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        
        // Skip comments and empty lines
        if (line.isEmpty() || line.startsWith("#")) {
            continue;
        }
        
        // Parse message line: TIMESTAMP | PGN | PRIORITY | SOURCE | DESTINATION | LENGTH | RAW_DATA
        // Handle both old format (TIMESTAMP|PGN|...) and new format (TIMESTAMP | PGN | ...)
        QStringList parts = line.split("|");
        if (parts.size() != 7) {
            skippedMessages++;
            continue;
        }
        
        // Extract message data (trim spaces for compatibility with new format)
        QString timestamp = parts[0].trimmed();
        QString pgnStr = parts[1].trimmed();
        QString priorityStr = parts[2].trimmed();
        QString sourceStr = parts[3].trimmed();
        QString destStr = parts[4].trimmed();
        QString lengthStr = parts[5].trimmed();
        QString rawDataStr = parts[6].trimmed();
        
        // Validate and convert data
        bool ok;
        uint32_t pgn = pgnStr.toUInt(&ok);
        if (!ok) {
            skippedMessages++;
            continue;
        }
        
        uint8_t priority = priorityStr.toUInt(&ok);
        if (!ok) priority = 6; // Default priority
        
        uint8_t source = sourceStr.toUInt(&ok, 16);
        if (!ok) source = 0;
        
        uint8_t destination = destStr.toUInt(&ok, 16);
        if (!ok) destination = 255;
        
        uint8_t dataLen = lengthStr.toUInt(&ok);
        if (!ok) dataLen = 0;
        
        // Parse raw data (space-separated hex bytes)
        QStringList hexBytes = rawDataStr.split(" ", Qt::SkipEmptyParts);
        if (hexBytes.size() != dataLen && !rawDataStr.isEmpty() && rawDataStr != "(no data)") {
            // Try to parse anyway, might be valid
        }
        
        // Reconstruct N2K message for decoding
        tN2kMsg reconstructedMsg;
        reconstructedMsg.PGN = pgn;
        reconstructedMsg.Priority = priority;
        reconstructedMsg.Source = source;
        reconstructedMsg.Destination = destination;
        reconstructedMsg.DataLen = qMin(dataLen, (uint8_t)hexBytes.size());
        
        // Parse hex data into byte array
        for (int i = 0; i < reconstructedMsg.DataLen && i < hexBytes.size(); i++) {
            bool hexOk;
            uint8_t byteVal = hexBytes[i].toUInt(&hexOk, 16);
            if (hexOk) {
                reconstructedMsg.Data[i] = byteVal;
            } else {
                reconstructedMsg.Data[i] = 0;
            }
        }
        
        // Add message to table (similar to appendMessage but without filtering)
        addLoadedMessage(reconstructedMsg, timestamp);
        loadedMessages++;
    }
    
    file.close();
    
    // Update status to clearly indicate a log has been loaded and live logging is stopped
    m_statusLabel->setText(QString("LOG LOADED (%1 messages) - Live logging STOPPED - Click Start to resume live logging").arg(loadedMessages));
    m_statusLabel->setStyleSheet("font-weight: bold; color: #0066cc; padding: 5px;");
    
    // Set loaded log state and update window title
    m_showingLoadedLog = true;
    QFileInfo fileInfo(fileName);
    m_loadedLogFileName = fileInfo.fileName();
    updateWindowTitle();
    
    // Show load result
    QMessageBox::information(this, "Load Complete", 
                           QString("PGN log loaded successfully!\n\nFile: %1\nLoaded: %2 messages\nSkipped: %3 messages\n\nMessages have been re-decoded with current DBC definitions.\n\nLive logging has been stopped.")
                           .arg(fileName)
                           .arg(loadedMessages)
                           .arg(skippedMessages));
}

void PGNLogDialog::addLoadedMessage(const tN2kMsg& msg, const QString& originalTimestamp)
{
    // Get decoded message name and data using current decoder
    QString messageName;
    if (m_dbcDecoder && m_dbcDecoder->canDecode(msg.PGN)) {
        messageName = m_dbcDecoder->getCleanMessageName(msg.PGN);
    } else {
        messageName = QString("PGN %1").arg(msg.PGN);
    }
    
    // Prepare decoded data
    QString decodedData = "";
    if (m_decodingEnabled->isChecked() && m_dbcDecoder && m_dbcDecoder->canDecode(msg.PGN)) {
        decodedData = m_dbcDecoder->getFormattedDecoded(msg);
        if (decodedData.isEmpty() || decodedData == "Raw data" || decodedData.startsWith("PGN")) {
            decodedData = "(not decoded)";
        }
    } else {
        decodedData = "(decoding disabled)";
    }
    
    // Format hex data payload
    QString hexData = "";
    for (int i = 0; i < msg.DataLen; i++) {
        if (i > 0) hexData += " ";
        hexData += QString("%1").arg(msg.Data[i], 2, 16, QChar('0')).toUpper();
    }
    if (hexData.isEmpty()) {
        hexData = "(no data)";
    }
    
    // Add new row to table
    int row = m_logTable->rowCount();
    m_logTable->insertRow(row);

    // Column 0: Timestamp (use original timestamp from file)
    QTableWidgetItem* tsItem = new QTableWidgetItem(originalTimestamp);
    tsItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_logTable->setItem(row, 0, tsItem);

    // Column 1: PGN
    QTableWidgetItem* pgnItem = new QTableWidgetItem(QString::number(msg.PGN));
    pgnItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_logTable->setItem(row, 1, pgnItem);

    // Column 2: Message Name (re-decoded with current definitions)
    QTableWidgetItem* nameItem = new QTableWidgetItem(messageName);
    nameItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_logTable->setItem(row, 2, nameItem);

    // Column 3: Priority
    QTableWidgetItem* priItem = new QTableWidgetItem(QString::number(msg.Priority));
    priItem->setTextAlignment(Qt::AlignCenter);
    m_logTable->setItem(row, 3, priItem);

    // Column 4: Source
    QTableWidgetItem* srcItem = new QTableWidgetItem(QString("%1").arg(msg.Source, 2, 16, QChar('0')).toUpper());
    srcItem->setTextAlignment(Qt::AlignCenter);
    m_logTable->setItem(row, 4, srcItem);

    // Column 5: Destination
    QString destText = QString("%1").arg(msg.Destination, 2, 16, QChar('0')).toUpper();
    QTableWidgetItem* destItem = new QTableWidgetItem(destText);
    destItem->setTextAlignment(Qt::AlignCenter);
    m_logTable->setItem(row, 5, destItem);

    // Column 6: Length
    QTableWidgetItem* lenItem = new QTableWidgetItem(QString::number(msg.DataLen));
    lenItem->setTextAlignment(Qt::AlignCenter);
    m_logTable->setItem(row, 6, lenItem);

    // Column 7: Raw Data
    QTableWidgetItem* rawDataItem = new QTableWidgetItem(hexData);
    rawDataItem->setFont(QFont("Consolas, Monaco, monospace", 9));
    m_logTable->setItem(row, 7, rawDataItem);

    // Column 8: Decoded Data (re-decoded with current DBC definitions)
    QTableWidgetItem* decodedItem = new QTableWidgetItem(decodedData);
    decodedItem->setFont(QFont("Consolas, Monaco, monospace", 9));
    m_logTable->setItem(row, 8, decodedItem);

    // Auto-scroll to bottom
    m_logTable->scrollToBottom();
}

void PGNLogDialog::setSourceFilter(uint8_t sourceAddress)
{
    m_sourceFilter = sourceAddress;
    m_sourceFilterActive = true;
    
    // Update UI
    m_sourceFilterEnabled->setChecked(true);
    
    // Find and select the appropriate item in the combo box
    for (int i = 0; i < m_sourceFilterCombo->count(); i++) {
        QString itemText = m_sourceFilterCombo->itemText(i);
        QString addressPattern = QString("0[xX]%1").arg(sourceAddress, 2, 16, QChar('0'));
        QRegExp regex(addressPattern, Qt::CaseInsensitive);
        if (regex.indexIn(itemText) >= 0) {
            m_sourceFilterCombo->setCurrentIndex(i);
            break;
        }
    }
    
    updateStatusLabel();
    
    // Clear existing log
    clearLog();
}

void PGNLogDialog::setDestinationFilter(uint8_t destinationAddress)
{
    m_destinationFilter = destinationAddress;
    m_destinationFilterActive = true;
    
    // Update UI
    m_destinationFilterEnabled->setChecked(true);
    
    // Find and select the appropriate item in the combo box
    for (int i = 0; i < m_destinationFilterCombo->count(); i++) {
        QString itemText = m_destinationFilterCombo->itemText(i);
        QString addressPattern = QString("0[xX]%1").arg(destinationAddress, 2, 16, QChar('0'));
        QRegExp regex(addressPattern, Qt::CaseInsensitive);
        if (regex.indexIn(itemText) >= 0) {
            m_destinationFilterCombo->setCurrentIndex(i);
            break;
        }
    }
    
    updateStatusLabel();
    
    // Clear existing log
    clearLog();
}

void PGNLogDialog::setFilterLogic(bool useOrLogic)
{
    m_useAndLogic = !useOrLogic; // Store as AND logic, so invert
    
    // Update UI combo box
    m_filterLogicCombo->setCurrentIndex(useOrLogic ? 1 : 0); // 0=AND, 1=OR
    
    updateStatusLabel();
}

void PGNLogDialog::updateDeviceList(const QStringList& devices)
{
    // Store current selections
    QString currentSource = m_sourceFilterCombo->currentText();
    QString currentDest = m_destinationFilterCombo->currentText();
    
    // Clear and rebuild combo boxes
    m_sourceFilterCombo->clear();
    m_destinationFilterCombo->clear();
    
    // Add default options
    m_sourceFilterCombo->addItem("Any");
    m_destinationFilterCombo->addItem("Any");
    m_destinationFilterCombo->addItem("Broadcast (255)");
    
    // Add device-specific options
    for (const QString& device : devices) {
        m_sourceFilterCombo->addItem(device);
        m_destinationFilterCombo->addItem(device);
    }
    
    // Add generic address options
    for (int i = 0; i < 254; i++) {
        QString addr = QString("0x%1 (%2)").arg(i, 2, 16, QChar('0')).toUpper().arg(i);
        if (!devices.contains(addr)) {
            m_sourceFilterCombo->addItem(addr);
            m_destinationFilterCombo->addItem(addr);
        }
    }
    
    // Restore selections if possible
    int sourceIndex = m_sourceFilterCombo->findText(currentSource);
    if (sourceIndex >= 0) {
        m_sourceFilterCombo->setCurrentIndex(sourceIndex);
    }
    
    int destIndex = m_destinationFilterCombo->findText(currentDest);
    if (destIndex >= 0) {
        m_destinationFilterCombo->setCurrentIndex(destIndex);
    }
}

void PGNLogDialog::onSourceFilterChanged()
{
    QString text = m_sourceFilterCombo->currentText();
    
    if (text == "Any") {
        m_sourceFilterActive = false; // Disable filtering when "Any" is selected
    } else {
        // Extract address from text (format: "0xXX (nnn)" or "Device Name (0xXX)")
        QRegExp regex("0[xX]([0-9A-F]{2})", Qt::CaseInsensitive);
        if (regex.indexIn(text) >= 0) {
            bool ok;
            uint8_t newFilter = regex.cap(1).toUInt(&ok, 16);
            if (ok) {
                m_sourceFilter = newFilter;
                m_sourceFilterActive = true;
            } else {
                m_sourceFilterActive = false;
            }
        } else {
            m_sourceFilterActive = false;
        }
    }
    
    updateStatusLabel();
}

void PGNLogDialog::onDestinationFilterChanged()
{
    QString text = m_destinationFilterCombo->currentText();
    //qDebug() << "onDestinationFilterChanged called with text:" << text;
    
    if (text == "Any") {
        m_destinationFilterActive = false; // Disable filtering when "Any" is selected
        //qDebug() << "Set destination filter to inactive (Any selected)";
    } else if (text.contains("Broadcast")) {
        m_destinationFilter = 255; // Actual broadcast address
        m_destinationFilterActive = true;
        //qDebug() << "Set destination filter to broadcast (255)";
    } else {
        // Extract address from text (format: "0xXX (nnn)" or "Device Name (0xXX)")
        QRegExp regex("0[xX]([0-9A-F]{2})", Qt::CaseInsensitive);
        if (regex.indexIn(text) >= 0) {
            bool ok;
            uint8_t newFilter = regex.cap(1).toUInt(&ok, 16);
            //qDebug() << "Extracted address from combo:" << QString("0x%1").arg(newFilter, 2, 16, QChar('0')).toUpper() << "ok:" << ok;
            if (ok) {
                m_destinationFilter = newFilter;
                m_destinationFilterActive = true;
                //qDebug() << "Set destination filter to:" << m_destinationFilter << "active:" << m_destinationFilterActive;
            } else {
                m_destinationFilterActive = false;
                //qDebug() << "Failed to parse address - disabling destination filter";
            }
        } else {
            m_destinationFilterActive = false;
            //qDebug() << "No address found in combo text - disabling destination filter";
        }
    }

    //qDebug() << "Final destination filter state - Active:" << m_destinationFilterActive << "Filter:" << m_destinationFilter;
    updateStatusLabel();
}

void PGNLogDialog::onSourceFilterEnabled(bool enabled)
{
    m_sourceFilterActive = enabled;
    m_sourceFilterCombo->setEnabled(enabled);
    
    if (enabled) {
        onSourceFilterChanged(); // Update filter value
    } else {
        m_sourceFilter = 255; // No filter
    }
    
    updateStatusLabel();
}

void PGNLogDialog::onDestinationFilterEnabled(bool enabled)
{
    m_destinationFilterActive = enabled;
    m_destinationFilterCombo->setEnabled(enabled);
    
    if (enabled) {
        onDestinationFilterChanged(); // Update filter value
    }
    
    updateStatusLabel();
}

void PGNLogDialog::onClearFilters()
{
    m_sourceFilterEnabled->setChecked(false);
    m_destinationFilterEnabled->setChecked(false);
    m_sourceFilterCombo->setCurrentIndex(0); // "Any"
    m_destinationFilterCombo->setCurrentIndex(0); // "Any"
    m_filterLogicCombo->setCurrentIndex(0); // "AND"
    
    m_sourceFilterActive = false;
    m_destinationFilterActive = false;
    m_sourceFilter = 255;
    m_destinationFilter = 255;
    m_useAndLogic = true;
    
    updateStatusLabel();
}

void PGNLogDialog::clearAllFilters()
{
    // Reset all filter states
    m_sourceFilterEnabled->setChecked(false);
    m_destinationFilterEnabled->setChecked(false);
    m_sourceFilterCombo->setCurrentIndex(0); // "Any"
    m_destinationFilterCombo->setCurrentIndex(0); // "Any"
    m_filterLogicCombo->setCurrentIndex(0); // "AND"
    
    m_sourceFilterActive = false;
    m_destinationFilterActive = false;
    m_sourceFilter = 255;
    m_destinationFilter = 255;
    m_useAndLogic = true;
    
    // Clear the log and add fresh header (this will handle window title)
    clearLog();
    
    updateStatusLabel();
}

void PGNLogDialog::onFilterLogicChanged()
{
    m_useAndLogic = (m_filterLogicCombo->currentIndex() == 0); // 0 = AND, 1 = OR
    //qDebug() << "Filter logic changed to:" << (m_useAndLogic ? "AND" : "OR");
    updateStatusLabel();
}

void PGNLogDialog::updateStatusLabel()
{
    // Don't override status if we're showing a loaded log file
    if (m_showingLoadedLog) {
        return;
    }
    
    QString status = "Live NMEA2000 PGN message log";
    
    QStringList activeFilters;
    if (m_sourceFilterActive) {
        activeFilters << QString("Source: 0x%1").arg(m_sourceFilter, 2, 16, QChar('0')).toUpper();
    }
    if (m_destinationFilterActive) {
        activeFilters << QString("Destination: 0x%1").arg(m_destinationFilter, 2, 16, QChar('0')).toUpper();
    }
    
    if (!activeFilters.isEmpty()) {
        QString logic = m_useAndLogic ? " (AND)" : " (OR)";
        if (activeFilters.size() > 1) {
            status += " - Filtered by " + activeFilters.join(", ") + logic;
        } else {
            status += " - Filtered by " + activeFilters.join(", ");
        }
    } else {
        status += " - Real-time updates";
    }
    
    m_statusLabel->setText(status);
}

bool PGNLogDialog::messagePassesFilter(const tN2kMsg& msg)
{
    bool sourceMatch = true;
    bool destMatch = true;
    
    // Check source filter
    if (m_sourceFilterActive) {
        sourceMatch = (msg.Source == m_sourceFilter);
        //qDebug() << "Source filter active - msg source:" << msg.Source << "filter:" << m_sourceFilter << "match:" << sourceMatch;
    } else {
        //qDebug() << "Source filter inactive";
    }
    
    // Check destination filter
    if (m_destinationFilterActive) {
        destMatch = (msg.Destination == m_destinationFilter);
        //qDebug() << "Dest filter active - msg dest:" << msg.Destination << "filter:" << m_destinationFilter << "match:" << destMatch;
    } else {
        //qDebug() << "Destination filter inactive";
    }
    
    // Apply logic
    bool result;
    if (m_useAndLogic) {
        // AND logic: both filters must pass (or be inactive)
        if (m_sourceFilterActive && m_destinationFilterActive) {
            result = sourceMatch && destMatch;
        } else if (m_sourceFilterActive) {
            result = sourceMatch;
        } else if (m_destinationFilterActive) {
            result = destMatch;
        } else {
            result = true; // No filters active
        }
        //qDebug() << "AND logic result:" << result;
    } else {
        // OR logic: at least one filter must pass (if any are active)
        if (m_sourceFilterActive && m_destinationFilterActive) {
            result = sourceMatch || destMatch;
        } else if (m_sourceFilterActive) {
            result = sourceMatch;
        } else if (m_destinationFilterActive) {
            result = destMatch;
        } else {
            result = true; // No filters active
        }
        //qDebug() << "OR logic result:" << result;
    }
    
    return result;
}

void PGNLogDialog::onToggleDecoding(bool enabled)
{
    // This slot is called when the decoding checkbox is toggled
    // No immediate action needed - the appendMessage functions will check the checkbox state
    Q_UNUSED(enabled);
}

void PGNLogDialog::onTableItemClicked(int row, int column)
{
    Q_UNUSED(column);
    Q_UNUSED(row);
    
    // When user clicks on a table row, pause the logging to allow review
    if (!m_logPaused && !m_logStopped) {
        onPauseClicked();
    }
}

void PGNLogDialog::onPauseClicked()
{
    m_logPaused = true;
    m_logStopped = false;
    
    // Update button states
    m_startButton->setEnabled(true);
    m_pauseButton->setEnabled(false);
    m_stopButton->setEnabled(true);
    
    // Update status
    m_statusLabel->setText("PAUSED - Click Start to resume logging");
    m_statusLabel->setStyleSheet("font-weight: bold; color: #ff6600; padding: 5px;");
}

void PGNLogDialog::onStartClicked()
{
    m_logPaused = false;
    m_logStopped = false;
    
    // Update button states
    m_startButton->setEnabled(false);
    m_pauseButton->setEnabled(true);
    m_stopButton->setEnabled(true);
    
    // Update status
    m_statusLabel->setText("Live NMEA2000 PGN message log - Real-time updates");
    m_statusLabel->setStyleSheet("font-weight: bold; color: #333; padding: 5px;");
    
    // Switch back to live mode
    m_showingLoadedLog = false;
    m_loadedLogFileName = "";
    updateWindowTitle();
}

void PGNLogDialog::onStopClicked()
{
    m_logPaused = false;
    m_logStopped = true;
    
    // Update button states
    m_startButton->setEnabled(true);
    m_pauseButton->setEnabled(false);
    m_stopButton->setEnabled(false);
    
    // Update status
    m_statusLabel->setText("STOPPED - Click Start to resume logging or Clear to empty log");
    m_statusLabel->setStyleSheet("font-weight: bold; color: #cc0000; padding: 5px;");
}

void PGNLogDialog::updateWindowTitle()
{
    if (m_showingLoadedLog) {
        setWindowTitle(QString("NMEA2000 PGN Message Log - LOADED: %1").arg(m_loadedLogFileName));
    } else {
        setWindowTitle("NMEA2000 PGN Message Log - LIVE");
    }
}

void PGNLogDialog::setTimestampMode(TimestampMode mode)
{
    if (m_timestampMode == mode) return;
    m_timestampMode = mode;
    // Update all timestamps in the table
    for (int row = 0; row < m_logTable->rowCount(); ++row) {
        QString tsText;
        if (mode == Absolute) {
            tsText = m_messageTimestamps[row].toString("HH:mm:ss.zzz");
        } else {
            qint64 deltaMs = 0;
            if (row > 0) {
                deltaMs = m_messageTimestamps[row-1].msecsTo(m_messageTimestamps[row]);
            }
            tsText = QString::number(deltaMs) + " ms";
        }
        QTableWidgetItem* tsItem = m_logTable->item(row, 0);
        if (tsItem) tsItem->setText(tsText);
    }
}

PGNLogDialog::TimestampMode PGNLogDialog::getTimestampMode() const
{
    return m_timestampMode;
}

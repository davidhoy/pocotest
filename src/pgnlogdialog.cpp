#include "pgnlogdialog.h"
#include <QMessageBox>
#include <QRegExp>
#include <QDebug>
#include <QFileDialog>
#include <QTextStream>
#include <QApplication>
#include <QScrollBar>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFont>
#include <QClipboard>
#include <QTimer>

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
    , m_autoScrollEnabled(true)  // Start with auto-scrolling enabled
    , m_userInteracting(false)   // User not initially interacting
    , m_dbcDecoder(nullptr)      // Will be initialized in constructor body
    , m_pgnIgnoreEdit(nullptr)
    , m_addPgnIgnoreButton(nullptr)
    , m_removePgnIgnoreButton(nullptr)
    , m_pgnIgnoreList(nullptr)
    , m_pgnFilteringEnabled(nullptr)
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
    
    // Load saved settings
    loadSettings();
    
    qDebug() << "PGNLogDialog constructor: Completed successfully";
}

PGNLogDialog::~PGNLogDialog()
{
    // Save settings when dialog is destroyed
    saveSettings();
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
    
    // Filter toolbar - single horizontal line
    QHBoxLayout* filterToolbar = new QHBoxLayout();
    
    qDebug() << "setupUI: Creating filter toolbar";
    
    // Source filter
    filterToolbar->addWidget(new QLabel("Source:"));
    m_sourceFilterCombo = new QComboBox();
    m_sourceFilterCombo->setMinimumWidth(150);
    filterToolbar->addWidget(m_sourceFilterCombo);
    
    qDebug() << "setupUI: Created source filter";
    
    // Add spacing
    filterToolbar->addSpacing(20);
    
    // Filter logic (between source and destination)
    filterToolbar->addWidget(new QLabel("Logic:"));
    m_filterLogicCombo = new QComboBox();
    m_filterLogicCombo->addItem("AND");
    m_filterLogicCombo->addItem("OR");
    m_filterLogicCombo->setCurrentIndex(0); // Default to AND
    m_filterLogicCombo->setMinimumWidth(80);
    filterToolbar->addWidget(m_filterLogicCombo);
    
    // Add spacing
    filterToolbar->addSpacing(20);
    
    // Destination filter
    filterToolbar->addWidget(new QLabel("Dest:"));
    m_destinationFilterCombo = new QComboBox();
    m_destinationFilterCombo->setMinimumWidth(150);
    filterToolbar->addWidget(m_destinationFilterCombo);
    
    qDebug() << "setupUI: Created destination filter";
    
    // Add spacing
    filterToolbar->addSpacing(20);
    
    // Clear filters button
    m_clearFiltersButton = new QPushButton("Clear Filters");
    filterToolbar->addWidget(m_clearFiltersButton);
    
    // Add stretch to push everything to the left
    filterToolbar->addStretch();
    
    mainLayout->addLayout(filterToolbar);
    
    // Initialize combo boxes with basic values (devices will be populated later)
    QStringList basicAddresses;
    basicAddresses << "Any" << "Broadcast (255)";
    
    m_sourceFilterCombo->addItems(basicAddresses);
    m_destinationFilterCombo->addItems(basicAddresses);
    
    // Connect filter signals
    connect(m_sourceFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &PGNLogDialog::onSourceFilterChanged);
    connect(m_destinationFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &PGNLogDialog::onDestinationFilterChanged);
    connect(m_filterLogicCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PGNLogDialog::onFilterLogicChanged);
    connect(m_clearFiltersButton, &QPushButton::clicked, this, &PGNLogDialog::onClearFilters);
    
    // PGN Filtering Section - Compact layout
    QGroupBox* pgnFilterGroup = new QGroupBox("PGN Filtering");
    pgnFilterGroup->setMaximumHeight(85);
    QHBoxLayout* pgnFilterLayout = new QHBoxLayout(pgnFilterGroup);
    pgnFilterLayout->setContentsMargins(6, 6, 6, 6);
    pgnFilterLayout->setSpacing(8);
    
    // Enable/disable checkbox
    m_pgnFilteringEnabled = new QCheckBox("Enable");
    m_pgnFilteringEnabled->setChecked(true);
    m_pgnFilteringEnabled->setToolTip("Enable or disable PGN message filtering");
    pgnFilterLayout->addWidget(m_pgnFilteringEnabled);
    pgnFilterLayout->addSpacing(8);
    
    // Left side: PGN input controls
    pgnFilterLayout->addWidget(new QLabel("Ignore PGN:"));
    m_pgnIgnoreEdit = new QLineEdit();
    m_pgnIgnoreEdit->setPlaceholderText("e.g., 127251");
    m_pgnIgnoreEdit->setFixedWidth(80);
    pgnFilterLayout->addWidget(m_pgnIgnoreEdit);
    
    m_addPgnIgnoreButton = new QPushButton("Add");
    m_addPgnIgnoreButton->setFixedWidth(45);
    pgnFilterLayout->addWidget(m_addPgnIgnoreButton);
    
    m_removePgnIgnoreButton = new QPushButton("Remove");
    m_removePgnIgnoreButton->setFixedWidth(60);
    pgnFilterLayout->addWidget(m_removePgnIgnoreButton);
    
    QPushButton* addCommonNoisyButton = new QPushButton("Add Common");
    addCommonNoisyButton->setFixedWidth(85);
    addCommonNoisyButton->setToolTip("Add frequently transmitted PGNs like position, heading, etc.");
    pgnFilterLayout->addWidget(addCommonNoisyButton);
    
    pgnFilterLayout->addSpacing(10);
    
    // Right side: Ignored PGNs list
    pgnFilterLayout->addWidget(new QLabel("Ignored:"));
    m_pgnIgnoreList = new QListWidget();
    m_pgnIgnoreList->setMaximumHeight(50);
    m_pgnIgnoreList->setToolTip("List of PGN message types being filtered out");
    pgnFilterLayout->addWidget(m_pgnIgnoreList);
    
    mainLayout->addWidget(pgnFilterGroup);
    
    // Connect PGN filter signals
    connect(m_addPgnIgnoreButton, &QPushButton::clicked, this, &PGNLogDialog::onAddPgnIgnore);
    connect(m_removePgnIgnoreButton, &QPushButton::clicked, this, &PGNLogDialog::onRemovePgnIgnore);
    connect(addCommonNoisyButton, &QPushButton::clicked, this, &PGNLogDialog::onAddCommonNoisyPgns);
    connect(m_pgnIgnoreEdit, &QLineEdit::returnPressed, this, &PGNLogDialog::onAddPgnIgnore);
    connect(m_pgnFilteringEnabled, &QCheckBox::toggled, this, &PGNLogDialog::onPgnFilteringToggled);
    
    // DBC Decoding and Timestamp options
    QHBoxLayout* optionsLayout = new QHBoxLayout();
    
    // Timestamp mode control
    m_timestampModeCheck = new QCheckBox("Show relative timestamps (ms)");
    m_timestampModeCheck->setChecked(false); // Default to absolute
    connect(m_timestampModeCheck, &QCheckBox::toggled, this, [this](bool checked) {
        setTimestampMode(checked ? Relative : Absolute);
    });
    optionsLayout->addWidget(m_timestampModeCheck);
    
    optionsLayout->addSpacing(20);

    // DBC Decoding option
    m_decodingEnabled = new QCheckBox("Enable DBC Decoding");
    m_decodingEnabled->setChecked(true); // Default to enabled
    m_decodingEnabled->setToolTip("Decode known NMEA2000 messages using DBC definitions");
    optionsLayout->addWidget(m_decodingEnabled);
    
    optionsLayout->addStretch();
    mainLayout->addLayout(optionsLayout);
    
    connect(m_decodingEnabled, &QCheckBox::toggled, this, &PGNLogDialog::onToggleDecoding);

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
    
    // Monitor scroll position to detect when user scrolls to bottom
    connect(m_logTable->verticalScrollBar(), &QScrollBar::valueChanged, 
            this, &PGNLogDialog::onScrollPositionChanged);
    
    // Enable context menu for right-click to add PGN to filter
    m_logTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_logTable, &QTableWidget::customContextMenuRequested, 
            this, &PGNLogDialog::onTableContextMenu);
    
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
    // Check if logging is stopped - if so, don't add new messages
    if (m_logStopped) {
        return; 
    }
    
    // If paused, continue adding messages but don't scroll (for examination)
    // If user is interacting, continue logging but don't auto-scroll
    
    // Apply filters
    if (!messagePassesFilter(msg)) {
        // Debug: Log when messages are filtered out
#if 0
        static int filteredCount = 0;
        filteredCount++;
        if (filteredCount % 10 == 0) { // Log every 10th filtered message to avoid spam
            qDebug() << "Message PGN" << msg.PGN << "filtered out (" << filteredCount << "total filtered)";
        }
#endif
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

    // Auto-scroll to bottom if enabled
    scrollToBottom();
}

void PGNLogDialog::appendSentMessage(const tN2kMsg& msg)
{
    // Check if logging is stopped - if so, don't add new messages
    if (m_logStopped) {
        return; 
    }
    
    // If paused or user is interacting, continue adding messages but don't scroll
    
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
    
    // Auto-scroll to bottom if enabled
    scrollToBottom();
}

void PGNLogDialog::clearLog()
{
    m_logTable->setRowCount(0);
    m_messageTimestamps.clear();
    
    // Reset to running state when clearing
    m_logPaused = false;
    m_logStopped = false;
    
    // Re-enable auto-scrolling when clearing
    m_autoScrollEnabled = true;
    m_userInteracting = false;
    
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
    out << "# Device names are included in decoded comments for readability\n";
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
        
        // Append current decoded information as human-readable comments with device names
        QString messageName = m_logTable->item(row, 2) ? m_logTable->item(row, 2)->text() : "";
        
        // Get device names for the comments section
        QString sourceName = "";
        QString destName = "";
        
        if (m_deviceNameResolver) {
            bool ok;
            uint8_t srcAddr = source.toUInt(&ok, 16);
            if (ok) {
                sourceName = m_deviceNameResolver(srcAddr);
            }
            
            uint8_t destAddr = destination.toUInt(&ok, 16);
            if (ok) {
                destName = m_deviceNameResolver(destAddr);
                if (destAddr == 255) {
                    destName = "Broadcast";
                }
            }
        }
        
        // Add device name information in comments
        if (!sourceName.isEmpty() || !destName.isEmpty()) {
            out << QString("#   Devices: %1 (0x%2) -> %3 (0x%4)\n")
                   .arg(sourceName.isEmpty() ? "Unknown" : sourceName)
                   .arg(source)
                   .arg(destName.isEmpty() ? "Unknown" : destName)
                   .arg(destination);
        }

        if (!messageName.isEmpty() && messageName != QString("PGN %1").arg(pgn)) {
            out << "#   Message: " << pgn << " - " << messageName << "\n";
        }        // Reconstruct message for clean decoding without reserved fields
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
        if (line.isEmpty() || line.startsWith("#") || line.startsWith("=")) {
            continue;
        }
        
        // Skip header lines
        if (line.contains("NMEA2000 PGN Message Log") || 
            line.contains("Generated by") || 
            line.contains("Export Time:") || 
            line.contains("Total Messages:") || 
            line.contains("Active Filters:") || 
            line.contains("Filter Logic:") ||
            (line.contains("Timestamp") && line.contains("PGN") && line.contains("Message Name"))) {
            continue;
        }
        
        // Detect format and parse accordingly
        tN2kMsg reconstructedMsg;
        QString timestamp;
        bool parseSuccess = false;
        
        // Try parsing as older format first (tab-delimited with decoded data)
        if (line.contains('\t') && !line.contains('|')) {
            parseSuccess = parseOlderFormatLine(line, reconstructedMsg, timestamp);
        }
        // Try parsing as newer format (pipe-delimited)
        else if (line.contains('|')) {
            parseSuccess = parseNewerFormatLine(line, reconstructedMsg, timestamp);
        }
        
        if (parseSuccess) {
            // Apply filtering to loaded messages (same as live messages)
            if (!messagePassesFilter(reconstructedMsg)) {
                skippedMessages++;
                continue; // Skip this message if it doesn't pass filters
            }
            
            // Add message to table
            addLoadedMessage(reconstructedMsg, timestamp);
            loadedMessages++;
        } else {
            skippedMessages++;
        }
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
    
    // Add device-specific options (only known devices)
    for (const QString& device : devices) {
        m_sourceFilterCombo->addItem(device);
        m_destinationFilterCombo->addItem(device);
    }
    
    // Note: Removed generic address options - only show currently known devices
    
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

void PGNLogDialog::onClearFilters()
{
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
    // First check PGN filtering - if enabled and PGN is ignored, reject immediately
    if (m_pgnFilteringEnabled && m_pgnFilteringEnabled->isChecked() && m_ignoredPgns.contains(msg.PGN)) {
        return false;
    }
    
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

void PGNLogDialog::refreshTableFilter()
{
    if (!m_logTable) {
        return;
    }
    
    qDebug() << "refreshTableFilter() called - starting filter update";
    
    int filteredCount = 0;
    int totalRows = m_logTable->rowCount();
    int visibleCount = 0;
    
    qDebug() << "Total rows to process:" << totalRows;
    
    // Temporarily disable updates to avoid flicker
    m_logTable->setUpdatesEnabled(false);
    
    // Track which rows are currently visible to minimize changes
    QVector<bool> currentlyVisible(totalRows);
    QVector<bool> shouldBeVisible(totalRows);
    
    // First pass: determine current visibility and calculate what should be visible
    for (int row = 0; row < totalRows; ++row) {
        currentlyVisible[row] = !m_logTable->isRowHidden(row);
        shouldBeVisible[row] = true;
        
        // Get PGN from column 1
        QTableWidgetItem* pgnItem = m_logTable->item(row, 1);
        if (pgnItem) {
            bool ok;
            uint32_t pgn = pgnItem->text().toUInt(&ok);
            bool pgnFilteringEnabled = m_pgnFilteringEnabled && m_pgnFilteringEnabled->isChecked();
            bool pgnShouldBeFiltered = ok && pgnFilteringEnabled && m_ignoredPgns.contains(pgn);
            
            if (row < 3) { // Debug first few rows
                qDebug() << "Row" << row << "PGN:" << pgn << "filtering enabled:" << pgnFilteringEnabled 
                         << "should filter:" << pgnShouldBeFiltered << "ignored PGNs:" << m_ignoredPgns;
            }
            
            if (pgnShouldBeFiltered) {
                shouldBeVisible[row] = false;
            }
        }
        
        // If PGN filtering passes, check source/destination filters
        if (shouldBeVisible[row]) {
            // Create a minimal tN2kMsg structure for filter checking
            tN2kMsg testMsg;
            
            // Get source from column 4
            QTableWidgetItem* srcItem = m_logTable->item(row, 4);
            if (srcItem) {
                bool ok;
                testMsg.Source = srcItem->text().toUInt(&ok, 16); // Parse as hex
                if (!ok) testMsg.Source = 255; // Default to broadcast
            }
            
            // Get destination from column 5  
            QTableWidgetItem* destItem = m_logTable->item(row, 5);
            if (destItem) {
                bool ok;
                testMsg.Destination = destItem->text().toUInt(&ok, 16); // Parse as hex
                if (!ok) testMsg.Destination = 255; // Default to broadcast
            }
            
            // Get PGN for the message structure
            if (pgnItem) {
                bool ok;
                testMsg.PGN = pgnItem->text().toUInt(&ok);
                if (!ok) testMsg.PGN = 0;
            }
            
            // Apply source/destination filtering logic (skip PGN check since we already did it)
            bool sourceMatch = true;
            bool destMatch = true;
            
            if (m_sourceFilterActive) {
                sourceMatch = (testMsg.Source == m_sourceFilter);
            }
            
            if (m_destinationFilterActive) {
                destMatch = (testMsg.Destination == m_destinationFilter);
            }
            
            // Apply logic
            if (m_useAndLogic) {
                if (m_sourceFilterActive && m_destinationFilterActive) {
                    shouldBeVisible[row] = sourceMatch && destMatch;
                } else if (m_sourceFilterActive) {
                    shouldBeVisible[row] = sourceMatch;
                } else if (m_destinationFilterActive) {
                    shouldBeVisible[row] = destMatch;
                }
            } else {
                if (m_sourceFilterActive && m_destinationFilterActive) {
                    shouldBeVisible[row] = sourceMatch || destMatch;
                } else if (m_sourceFilterActive) {
                    shouldBeVisible[row] = sourceMatch;
                } else if (m_destinationFilterActive) {
                    shouldBeVisible[row] = destMatch;
                }
            }
        }
        
        if (shouldBeVisible[row]) {
            visibleCount++;
        } else {
            filteredCount++;
        }
    }
    
    qDebug() << "Filter analysis: " << visibleCount << " rows should be visible, " << filteredCount << " should be hidden";
    
    // Second pass: only change visibility for rows that need it
    int changedRows = 0;
    for (int row = 0; row < totalRows; ++row) {
        if (currentlyVisible[row] != shouldBeVisible[row]) {
            m_logTable->setRowHidden(row, !shouldBeVisible[row]);
            changedRows++;
        }
    }
    
    qDebug() << "Visibility changes: " << changedRows << " rows changed";
    
    // Re-enable updates with a proper repaint
    m_logTable->setUpdatesEnabled(true);
    
    // Force a viewport update to ensure proper rendering
    if (changedRows > 0) {
        m_logTable->viewport()->update();
        
        // If we have visible rows, make sure scrolling still works
        if (visibleCount > 0 && m_autoScrollEnabled) {
            QTimer::singleShot(0, this, &PGNLogDialog::scrollToBottom);
        }
    }
    
    // Update status to show filtering information
    if (filteredCount > 0) {
        qDebug() << "PGN filtering: " << filteredCount << " messages hidden from view (out of " << totalRows << " total)";
    } else {
        qDebug() << "PGN filtering: No messages filtered (all" << totalRows << "messages visible)";
    }
    
    updateStatusLabel();
}

void PGNLogDialog::onTableItemClicked(int row, int column)
{
    Q_UNUSED(column);
    Q_UNUSED(row);
    
    // User clicked on a row - disable auto-scrolling to let them examine the message
    m_autoScrollEnabled = false;
    m_userInteracting = true;
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
    
    // Re-enable auto-scrolling when starting
    m_autoScrollEnabled = true;
    m_userInteracting = false;
    
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

bool PGNLogDialog::parseOlderFormatLine(const QString& line, tN2kMsg& msg, QString& timestamp)
{
    // Parse older format: Timestamp\tPGN\tMessage Name\tPri\tSrc\tDst\tLen\tData
    // Example: 14:21:27.257	126993	Heartbeat	7	5C	FF	8	[decoded info] [60 EA 1F C0 FF FF FF FF]
    
    QStringList parts = line.split('\t');
    if (parts.size() < 7) {
        return false; // Not enough fields
    }
    
    timestamp = parts[0].trimmed();
    QString pgnStr = parts[1].trimmed();
    // parts[2] is message name (skip)
    QString priorityStr = parts[3].trimmed();
    QString sourceStr = parts[4].trimmed();
    QString destStr = parts[5].trimmed();
    QString lengthStr = parts[6].trimmed();
    
    // Extract hex data from the last field - look for pattern [XX XX XX ...]
    QString dataField = parts.size() > 7 ? parts[7] : "";
    QString hexDataStr = "";
    
    // Find hex data in brackets at the end
    QRegExp hexPattern(R"(\[([0-9A-Fa-f\s]+)\])");
    if (hexPattern.indexIn(dataField) >= 0) {
        hexDataStr = hexPattern.cap(1).trimmed();
    }
    
    // Validate and convert data
    bool ok;
    uint32_t pgn = pgnStr.toUInt(&ok);
    if (!ok) return false;
    
    uint8_t priority = priorityStr.toUInt(&ok);
    if (!ok) priority = 6; // Default priority
    
    uint8_t source = sourceStr.toUInt(&ok, 16);
    if (!ok) source = 0;
    
    uint8_t destination = destStr.toUInt(&ok, 16);
    if (!ok) destination = 255;
    
    uint8_t dataLen = lengthStr.toUInt(&ok);
    if (!ok) dataLen = 0;
    
    // Parse hex data
    QStringList hexBytes = hexDataStr.split(QRegExp("\\s+"), Qt::SkipEmptyParts);
    
    // Build the message
    msg.PGN = pgn;
    msg.Priority = priority;
    msg.Source = source;
    msg.Destination = destination;
    msg.DataLen = qMin(dataLen, (uint8_t)hexBytes.size());
    
    // Parse hex data into byte array
    for (int i = 0; i < msg.DataLen && i < hexBytes.size(); i++) {
        bool hexOk;
        uint8_t byteVal = hexBytes[i].toUInt(&hexOk, 16);
        if (hexOk) {
            msg.Data[i] = byteVal;
        } else {
            msg.Data[i] = 0;
        }
    }
    
    return true;
}

bool PGNLogDialog::parseNewerFormatLine(const QString& line, tN2kMsg& msg, QString& timestamp)
{
    // Parse formats - try newest format first (with device names), then fall back to older
    QStringList parts = line.split("|");
    
    if (parts.size() == 9) {
        // Newest format: TIMESTAMP | PGN | PRIORITY | SOURCE | SOURCE_NAME | DESTINATION | DEST_NAME | LENGTH | RAW_DATA
        timestamp = parts[0].trimmed();
        QString pgnStr = parts[1].trimmed();
        QString priorityStr = parts[2].trimmed();
        QString sourceStr = parts[3].trimmed();
        // parts[4] is source name (we ignore for parsing)
        QString destStr = parts[5].trimmed();
        // parts[6] is dest name (we ignore for parsing) 
        QString lengthStr = parts[7].trimmed();
        QString rawDataStr = parts[8].trimmed();
        
        // Validate and convert data
        bool ok;
        uint32_t pgn = pgnStr.toUInt(&ok);
        if (!ok) return false;
        
        uint8_t priority = priorityStr.toUInt(&ok);
        if (!ok) priority = 6; // Default priority
        
        uint8_t source = sourceStr.toUInt(&ok, 16);
        if (!ok) source = 0;
        
        uint8_t destination = destStr.toUInt(&ok, 16);
        if (!ok) destination = 255;
        
        uint8_t dataLen = lengthStr.toUInt(&ok);
        if (!ok) dataLen = 0;
        
        // Parse hex data
        QStringList hexBytes = rawDataStr.split(" ", Qt::SkipEmptyParts);
        
        // Construct N2K message
        msg.PGN = pgn;
        msg.Priority = priority;
        msg.Source = source;
        msg.Destination = destination;
        msg.DataLen = qMin(dataLen, (uint8_t)8);
        
        // Parse data bytes
        for (int i = 0; i < msg.DataLen && i < hexBytes.size(); i++) {
            bool hexOk;
            uint8_t byteVal = hexBytes[i].toUInt(&hexOk, 16);
            if (hexOk) {
                msg.Data[i] = byteVal;
            } else {
                msg.Data[i] = 0;
            }
        }
        
        return true;
    }
    else if (parts.size() == 7) {
        // Older format: TIMESTAMP | PGN | PRIORITY | SOURCE | DESTINATION | LENGTH | RAW_DATA
        timestamp = parts[0].trimmed();
        QString pgnStr = parts[1].trimmed();
        QString priorityStr = parts[2].trimmed();
        QString sourceStr = parts[3].trimmed();
        QString destStr = parts[4].trimmed();
        QString lengthStr = parts[5].trimmed();
        QString rawDataStr = parts[6].trimmed();
        
        // Validate and convert data (same as before)
        bool ok;
        uint32_t pgn = pgnStr.toUInt(&ok);
        if (!ok) return false;
        
        uint8_t priority = priorityStr.toUInt(&ok);
        if (!ok) priority = 6; // Default priority
        
        uint8_t source = sourceStr.toUInt(&ok, 16);
        if (!ok) source = 0;
        
        uint8_t destination = destStr.toUInt(&ok, 16);
        if (!ok) destination = 255;
        
        uint8_t dataLen = lengthStr.toUInt(&ok);
        if (!ok) dataLen = 0;
        
        // Parse hex data
        QStringList hexBytes = rawDataStr.split(" ", Qt::SkipEmptyParts);
        
        // Construct N2K message
        msg.PGN = pgn;
        msg.Priority = priority;
        msg.Source = source;
        msg.Destination = destination;
        msg.DataLen = qMin(dataLen, (uint8_t)8);
        
        // Parse data bytes
        for (int i = 0; i < msg.DataLen && i < hexBytes.size(); i++) {
            bool hexOk;
            uint8_t byteVal = hexBytes[i].toUInt(&hexOk, 16);
            if (hexOk) {
                msg.Data[i] = byteVal;
            } else {
                msg.Data[i] = 0;
            }
        }
        
        return true;
    }
    
    return false; // Unsupported format
}


// PGN Filtering Methods
void PGNLogDialog::onAddPgnIgnore()
{
    bool ok;
    QString text = m_pgnIgnoreEdit->text().trimmed();
    uint32_t pgn = text.toUInt(&ok);
    
    if (!ok || pgn == 0) {
        QMessageBox::warning(this, "Invalid PGN", "Please enter a valid PGN number (e.g., 127251)");
        return;
    }
    
    if (m_ignoredPgns.contains(pgn)) {
        QMessageBox::information(this, "PGN Already Ignored", QString("PGN %1 is already in the ignore list.").arg(pgn));
        return;
    }
    
    addPgnToIgnoreList(pgn);
    m_pgnIgnoreEdit->clear();
    updateStatusLabel();
}

void PGNLogDialog::onRemovePgnIgnore()
{
    QListWidgetItem* currentItem = m_pgnIgnoreList->currentItem();
    if (!currentItem) {
        QMessageBox::information(this, "No Selection", "Please select a PGN from the list to remove.");
        return;
    }
    
    QString itemText = currentItem->text();
    QString pgnStr = itemText.split(" ").first();
    bool ok;
    uint32_t pgn = pgnStr.toUInt(&ok);
    
    if (ok) {
        removePgnFromIgnoreList(pgn);
        updateStatusLabel();
    }
}

void PGNLogDialog::onAddCommonNoisyPgns()
{
    QSet<uint32_t> commonNoisyPgns = {
        127250, 127251, 127257, 127258, 128259, 128267,
        129025, 129026, 129029, 129033, 129539, 129540
    };
    
    int addedCount = 0;
    for (uint32_t pgn : commonNoisyPgns) {
        if (!m_ignoredPgns.contains(pgn)) {
            addPgnToIgnoreList(pgn);
            addedCount++;
        }
    }
    
    if (addedCount > 0) {
        updateStatusLabel();
        QMessageBox::information(this, "Common Noisy PGNs Added", 
                               QString("Added %1 common noisy PGNs to the ignore list.").arg(addedCount));
    }
}

void PGNLogDialog::addPgnToIgnoreList(uint32_t pgn)
{
    qDebug() << "addPgnToIgnoreList() called for PGN:" << pgn;
    qDebug() << "Current ignored PGNs before adding:" << m_ignoredPgns;
    
    m_ignoredPgns.insert(pgn);
    
    qDebug() << "Current ignored PGNs after adding:" << m_ignoredPgns;
    
    QString displayText = QString::number(pgn);
    if (m_dbcDecoder && m_dbcDecoder->canDecode(pgn)) {
        QString messageName = m_dbcDecoder->getCleanMessageName(pgn);
        if (!messageName.isEmpty() && !messageName.startsWith("PGN")) {
            displayText += QString(" (%1)").arg(messageName);
        }
    }
    m_pgnIgnoreList->addItem(displayText);
    m_pgnIgnoreList->sortItems();
    
    qDebug() << "About to call updateStatusLabel() and refreshTableFilter()";
    updateStatusLabel();
    refreshTableFilter(); // Apply new filter to existing table rows
    qDebug() << "Filter refresh completed, about to save settings";
    saveSettings(); // Persist the change
    qDebug() << "addPgnToIgnoreList() completed";
}

void PGNLogDialog::removePgnFromIgnoreList(uint32_t pgn)
{
    m_ignoredPgns.remove(pgn);
    
    for (int i = 0; i < m_pgnIgnoreList->count(); i++) {
        QListWidgetItem* item = m_pgnIgnoreList->item(i);
        QString itemText = item->text();
        QString pgnStr = itemText.split(" ").first();
        bool ok;
        uint32_t itemPgn = pgnStr.toUInt(&ok);
        if (ok && itemPgn == pgn) {
            delete m_pgnIgnoreList->takeItem(i);
            break;
        }
    }
    
    updateStatusLabel();
    refreshTableFilter(); // Apply updated filter to existing table rows
    saveSettings(); // Persist the change
}

void PGNLogDialog::setIgnoredPgns(const QSet<uint32_t>& pgns)
{
    m_ignoredPgns = pgns;
    
    m_pgnIgnoreList->clear();
    for (uint32_t pgn : m_ignoredPgns) {
        QString displayText = QString::number(pgn);
        if (m_dbcDecoder && m_dbcDecoder->canDecode(pgn)) {
            QString messageName = m_dbcDecoder->getCleanMessageName(pgn);
            if (!messageName.isEmpty() && !messageName.startsWith("PGN")) {
                displayText += QString(" (%1)").arg(messageName);
            }
        }
        m_pgnIgnoreList->addItem(displayText);
    }
    m_pgnIgnoreList->sortItems();
    
    updateStatusLabel();
    refreshTableFilter(); // Apply updated filters to existing table rows
}

void PGNLogDialog::onTableContextMenu(const QPoint& position)
{
    QTableWidgetItem* item = m_logTable->itemAt(position);
    if (!item) {
        return; // No item at this position
    }
    
    int row = item->row();
    
    // Get the PGN from column 1 (PGN column)
    QTableWidgetItem* pgnItem = m_logTable->item(row, 1);
    if (!pgnItem) {
        return; // No PGN data in this row
    }
    
    bool ok;
    uint32_t pgn = pgnItem->text().toUInt(&ok);
    if (!ok) {
        return; // Invalid PGN value
    }
    
    // Get message name from column 2 for display
    QString messageName;
    QTableWidgetItem* nameItem = m_logTable->item(row, 2);
    if (nameItem) {
        messageName = nameItem->text();
    }
    
    // Create context menu
    QMenu* contextMenu = new QMenu(this);
    
    // Add decode details option first
    QString detailsText = QString("Show decode details for PGN %1").arg(pgn);
    if (!messageName.isEmpty() && !messageName.startsWith("PGN")) {
        detailsText += QString(" (%1)").arg(messageName);
    }
    
    QAction* detailsAction = contextMenu->addAction(detailsText);
    connect(detailsAction, &QAction::triggered, [this, row]() {
        showDecodeDetails(row);
    });
    
    // Add separator
    contextMenu->addSeparator();
    
    // Add existing filter option
    QString menuText;
    if (m_ignoredPgns.contains(pgn)) {
        // PGN is already in ignore list - offer to remove it
        menuText = QString("Remove PGN %1 from filter list").arg(pgn);
        if (!messageName.isEmpty() && !messageName.startsWith("PGN")) {
            menuText += QString(" (%1)").arg(messageName);
        }
        
        QAction* removeAction = contextMenu->addAction(menuText);
        connect(removeAction, &QAction::triggered, [this, pgn]() {
            removePgnFromIgnoreList(pgn);
        });
    } else {
        // PGN is not in ignore list - offer to add it
        menuText = QString("Add PGN %1 to filter list").arg(pgn);
        if (!messageName.isEmpty() && !messageName.startsWith("PGN")) {
            menuText += QString(" (%1)").arg(messageName);
        }
        
        QAction* addAction = contextMenu->addAction(menuText);
        connect(addAction, &QAction::triggered, [this, pgn]() {
            qDebug() << "Context menu: User selected to exclude PGN" << pgn;
            addPgnToIgnoreList(pgn);
        });
    }
    
    // Show the context menu at the cursor position
    contextMenu->exec(m_logTable->mapToGlobal(position));
    
    // Clean up
    contextMenu->deleteLater();
}

void PGNLogDialog::onPgnFilteringToggled(bool enabled)
{
    // Enable/disable PGN filtering controls
    m_pgnIgnoreEdit->setEnabled(enabled);
    m_addPgnIgnoreButton->setEnabled(enabled);
    m_removePgnIgnoreButton->setEnabled(enabled);
    m_pgnIgnoreList->setEnabled(enabled);
    
    // Refresh the table to apply/remove PGN filtering
    refreshTableFilter();
    
    // Save the setting immediately
    saveSettings();
}

void PGNLogDialog::saveSettings()
{
    QSettings settings;
    settings.beginGroup("PGNLogDialog");
    
    // Save PGN filtering enabled state
    if (m_pgnFilteringEnabled) {
        settings.setValue("pgnFilteringEnabled", m_pgnFilteringEnabled->isChecked());
    }
    
    // Save ignored PGNs list
    QStringList pgnList;
    for (uint32_t pgn : m_ignoredPgns) {
        pgnList << QString::number(pgn);
    }
    settings.setValue("ignoredPgns", pgnList);
    
    settings.endGroup();
}

void PGNLogDialog::loadSettings()
{
    QSettings settings;
    settings.beginGroup("PGNLogDialog");
    
    // Load PGN filtering enabled state
    bool pgnFilteringEnabled = settings.value("pgnFilteringEnabled", true).toBool();
    if (m_pgnFilteringEnabled) {
        m_pgnFilteringEnabled->setChecked(pgnFilteringEnabled);
        // Apply the initial state without triggering save
        m_pgnIgnoreEdit->setEnabled(pgnFilteringEnabled);
        m_addPgnIgnoreButton->setEnabled(pgnFilteringEnabled);
        m_removePgnIgnoreButton->setEnabled(pgnFilteringEnabled);
        m_pgnIgnoreList->setEnabled(pgnFilteringEnabled);
        refreshTableFilter();
    }
    
    // Load ignored PGNs list
    QStringList pgnList = settings.value("ignoredPgns", QStringList()).toStringList();
    QSet<uint32_t> loadedPgns;
    
    for (const QString& pgnStr : pgnList) {
        bool ok;
        uint32_t pgn = pgnStr.toUInt(&ok);
        if (ok) {
            loadedPgns.insert(pgn);
        }
    }
    
    if (!loadedPgns.isEmpty()) {
        setIgnoredPgns(loadedPgns);
    }
    
    settings.endGroup();
}

void PGNLogDialog::onScrollPositionChanged()
{
    // Check if user has scrolled to the bottom
    if (isScrolledToBottom()) {
        // Re-enable auto-scrolling when user scrolls to bottom
        m_autoScrollEnabled = true;
        m_userInteracting = false;
    } else {
        // User has scrolled away from bottom - disable auto-scrolling
        if (!m_userInteracting) {
            m_autoScrollEnabled = false;
            m_userInteracting = true;
        }
    }
}

bool PGNLogDialog::isScrolledToBottom() const
{
    QScrollBar* scrollBar = m_logTable->verticalScrollBar();
    // Consider it "at bottom" if within a few pixels of the maximum
    return (scrollBar->value() >= scrollBar->maximum() - 3);
}

void PGNLogDialog::scrollToBottom()
{
    if (m_autoScrollEnabled && !m_logStopped && !m_logPaused) {
        m_logTable->scrollToBottom();
    }
}

void PGNLogDialog::setDeviceNameResolver(DeviceNameResolver resolver)
{
    m_deviceNameResolver = resolver;
}

void PGNLogDialog::showDecodeDetails(int row)
{
    if (row < 0 || row >= m_logTable->rowCount()) {
        return; // Invalid row
    }
    
    // Extract message data from the table row
    QString timestamp = m_logTable->item(row, 0) ? m_logTable->item(row, 0)->text() : "";
    QString pgnText = m_logTable->item(row, 1) ? m_logTable->item(row, 1)->text() : "";
    QString messageName = m_logTable->item(row, 2) ? m_logTable->item(row, 2)->text() : "";
    QString priority = m_logTable->item(row, 3) ? m_logTable->item(row, 3)->text() : "";
    QString source = m_logTable->item(row, 4) ? m_logTable->item(row, 4)->text() : "";
    QString destination = m_logTable->item(row, 5) ? m_logTable->item(row, 5)->text() : "";
    QString length = m_logTable->item(row, 6) ? m_logTable->item(row, 6)->text() : "";
    QString rawData = m_logTable->item(row, 7) ? m_logTable->item(row, 7)->text() : "";
    QString decodedData = m_logTable->item(row, 8) ? m_logTable->item(row, 8)->text() : "";
    
    // Clean up PGN field (remove "Sent:" prefix if present)
    QString pgn = pgnText;
    if (pgn.startsWith("Sent: ")) {
        pgn = pgn.mid(6);
    }
    
    // Create the details dialog
    QDialog* detailsDialog = new QDialog(this);
    detailsDialog->setWindowTitle(QString("Message Details - PGN %1").arg(pgn));
    detailsDialog->setMinimumSize(600, 400);
    detailsDialog->resize(700, 500);
    
    // Create layout
    QVBoxLayout* layout = new QVBoxLayout(detailsDialog);
    
    // Create text display area
    QTextEdit* textDisplay = new QTextEdit(detailsDialog);
    textDisplay->setReadOnly(true);
    textDisplay->setFont(QFont("Courier", 10)); // Monospace font for better alignment
    
    // Build the detailed information text
    QString detailsText;
    
    // Header information
    detailsText += "NMEA2000 Message Details\n";
    detailsText += "========================\n\n";
    
    // Basic message information
    detailsText += "Message Information:\n";
    detailsText += "-------------------\n";
    detailsText += QString("Timestamp:    %1\n").arg(timestamp);
    detailsText += QString("PGN:          %1").arg(pgn);
    if (!messageName.isEmpty() && messageName != QString("PGN %1").arg(pgn)) {
        detailsText += QString(" (%1)").arg(messageName);
    }
    detailsText += "\n";
    detailsText += QString("Priority:     %1\n").arg(priority);
    detailsText += QString("Source:       0x%1").arg(source);
    detailsText += QString("Destination:  0x%1").arg(destination);
    detailsText += QString("Length:       %1 bytes\n").arg(length);
    
    // Add device name information if available
    if (m_deviceNameResolver) {
        bool ok;
        uint8_t srcAddr = source.toUInt(&ok, 16);
        if (ok) {
            QString sourceName = m_deviceNameResolver(srcAddr);
            if (!sourceName.isEmpty()) {
                detailsText += QString("Source Device: %1\n").arg(sourceName);
            }
        }
        
        uint8_t destAddr = destination.toUInt(&ok, 16);
        if (ok) {
            QString destName = m_deviceNameResolver(destAddr);
            if (destAddr == 255) {
                destName = "Broadcast";
            }
            if (!destName.isEmpty()) {
                detailsText += QString("Dest Device:   %1\n").arg(destName);
            }
        }
    }
    
    detailsText += "\n";
    
    // Raw data section
    detailsText += "Raw Data:\n";
    detailsText += "---------\n";
    if (!rawData.isEmpty() && rawData != "(no data)") {
        // Format raw data with better spacing
        QStringList hexBytes = rawData.split(" ", Qt::SkipEmptyParts);
        QString formattedRawData;
        for (int i = 0; i < hexBytes.size(); i++) {
            if (i > 0 && i % 8 == 0) {
                formattedRawData += "\n          ";
            }
            formattedRawData += hexBytes[i].rightJustified(2, '0').toUpper() + " ";
        }
        detailsText += QString("Hex:      %1\n").arg(formattedRawData.trimmed());
        
        // Add byte positions for reference
        QString bytePositions;
        for (int i = 0; i < hexBytes.size(); i++) {
            if (i > 0 && i % 8 == 0) {
                bytePositions += "\n          ";
            }
            bytePositions += QString("%1  ").arg(i, 2);
        }
        detailsText += QString("Positions:%1\n").arg(bytePositions);
    } else {
        detailsText += "No data\n";
    }
    
    detailsText += "\n";
    
    // Decoded data section
    detailsText += "Decoded Information:\n";
    detailsText += "-------------------\n";
    
    // Try to get enhanced decoded data if decoder is available
    if (m_dbcDecoder && m_decodingEnabled->isChecked() && !rawData.isEmpty() && rawData != "(no data)") {
        bool ok;
        uint32_t pgnNum = pgn.toUInt(&ok);
        if (ok && m_dbcDecoder->canDecode(pgnNum)) {
            // Reconstruct the message for detailed decoding
            QStringList hexBytes = rawData.split(" ", Qt::SkipEmptyParts);
            uint8_t dataLen = length.toUInt(&ok);
            if (ok) {
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
                
                // Get detailed decoded information
                QString detailedDecoded = m_dbcDecoder->getFormattedDecodedForSave(reconstructedMsg);
                if (!detailedDecoded.isEmpty() && detailedDecoded != "Raw data" && detailedDecoded != "(not decoded)") {
                    // Format the decoded data with better line breaks
                    QStringList decodedParts = detailedDecoded.split(", ");
                    for (const QString& part : decodedParts) {
                        detailsText += QString("  %1\n").arg(part);
                    }
                } else {
                    detailsText += "Message structure recognized but no decoded data available\n";
                }
            }
        } else {
            detailsText += "No decoder available for this PGN\n";
        }
    } else {
        if (!decodedData.isEmpty() && decodedData != "(not decoded)" && decodedData != "Raw data") {
            detailsText += QString("%1\n").arg(decodedData);
        } else {
            detailsText += "No decoded information available\n";
        }
    }
    
    // Set the text in the display
    textDisplay->setPlainText(detailsText);
    
    // Add to layout
    layout->addWidget(textDisplay);
    
    // Create button layout
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    QPushButton* copyButton = new QPushButton("Copy to Clipboard", detailsDialog);
    connect(copyButton, &QPushButton::clicked, [textDisplay]() {
        QApplication::clipboard()->setText(textDisplay->toPlainText());
    });
    buttonLayout->addWidget(copyButton);
    
    QPushButton* closeButton = new QPushButton("Close", detailsDialog);
    connect(closeButton, &QPushButton::clicked, detailsDialog, &QDialog::accept);
    buttonLayout->addWidget(closeButton);
    
    layout->addLayout(buttonLayout);
    
    // Show the dialog
    detailsDialog->exec();
    detailsDialog->deleteLater();
}

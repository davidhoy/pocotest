#include "pgnlogdialog.h"
#include <QMessageBox>
#include <QRegExp>

PGNLogDialog::PGNLogDialog(QWidget *parent)
    : QDialog(parent)
    , m_logTextEdit(nullptr)
    , m_clearButton(nullptr)
    , m_closeButton(nullptr)
    , m_clearFiltersButton(nullptr)
    , m_statusLabel(nullptr)
    , m_filterGroup(nullptr)
    , m_sourceFilterEnabled(nullptr)
    , m_destinationFilterEnabled(nullptr)
    , m_sourceFilterCombo(nullptr)
    , m_destinationFilterCombo(nullptr)
    , m_sourceFilter(255)        // No filter initially
    , m_destinationFilter(255)   // No filter initially
    , m_sourceFilterActive(false)
    , m_destinationFilterActive(false)
{
    setupUI();
    
    setWindowTitle("NMEA2000 PGN Message Log");
    setModal(false);
    resize(900, 700);
}

PGNLogDialog::~PGNLogDialog()
{
}

void PGNLogDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Status label
    m_statusLabel = new QLabel("Live NMEA2000 PGN message log - Real-time updates");
    m_statusLabel->setStyleSheet("font-weight: bold; color: #333; padding: 5px;");
    mainLayout->addWidget(m_statusLabel);
    
    // Filter group box
    m_filterGroup = new QGroupBox("Message Filters");
    QVBoxLayout* filterLayout = new QVBoxLayout(m_filterGroup);
    
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
    connect(m_clearFiltersButton, &QPushButton::clicked, this, &PGNLogDialog::onClearFilters);
    
    // Log text edit
    m_logTextEdit = new QTextEdit();
    m_logTextEdit->setLineWrapMode(QTextEdit::WidgetWidth);
    m_logTextEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_logTextEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_logTextEdit->setFont(QFont("Consolas, Monaco, monospace", 9));
    
    // Add some initial content
    m_logTextEdit->append("NMEA2000 PGN Message Log");
    m_logTextEdit->append("Ready to receive and display CAN messages");
    m_logTextEdit->append("===========================================");
    
    mainLayout->addWidget(m_logTextEdit);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    m_clearButton = new QPushButton("Clear Log");
    m_closeButton = new QPushButton("Close");
    
    connect(m_clearButton, &QPushButton::clicked, this, &PGNLogDialog::clearLog);
    connect(m_closeButton, &QPushButton::clicked, this, &PGNLogDialog::onCloseClicked);
    
    buttonLayout->addWidget(m_clearButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_closeButton);
    
    mainLayout->addLayout(buttonLayout);
}

void PGNLogDialog::appendMessage(const tN2kMsg& msg)
{
    // Apply filters
    if (!messagePassesFilter(msg)) {
        return; // Skip this message
    }
    
    QString pgnInfo = QString("PGN: %1, Priority: %2, Source: 0x%3, Destination: %4")
                          .arg(msg.PGN)
                          .arg(msg.Priority)
                          .arg(msg.Source, 2, 16, QChar('0')).toUpper()
                          .arg(msg.Destination == 255 ? "Broadcast" : QString("0x%1").arg(msg.Destination, 2, 16, QChar('0')).toUpper());

    // Append to the scrolling text box
    m_logTextEdit->append(pgnInfo);
    
    // Auto-scroll to bottom
    QTextCursor cursor = m_logTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logTextEdit->setTextCursor(cursor);
}

void PGNLogDialog::clearLog()
{
    m_logTextEdit->clear();
    m_logTextEdit->append("NMEA2000 PGN Message Log - Log cleared");
    m_logTextEdit->append("Ready to receive and display CAN messages");
    m_logTextEdit->append("===========================================");
}

void PGNLogDialog::onCloseClicked()
{
    hide(); // Hide instead of close so it can be reopened
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
        if (itemText.contains(QString("0x%1").arg(sourceAddress, 2, 16, QChar('0')).toUpper())) {
            m_sourceFilterCombo->setCurrentIndex(i);
            break;
        }
    }
    
    updateStatusLabel();
    
    // Clear existing log and add new header
    clearLog();
    m_logTextEdit->append(QString("Filtering messages from device 0x%1")
                         .arg(sourceAddress, 2, 16, QChar('0')).toUpper());
    m_logTextEdit->append("===========================================");
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
    if (!m_sourceFilterActive) return;
    
    QString text = m_sourceFilterCombo->currentText();
    if (text == "Any") {
        m_sourceFilter = 255;
    } else {
        // Extract address from text (format: "0xXX (nnn)" or "Device Name (0xXX)")
        QRegExp regex("0x([0-9A-F]{2})");
        if (regex.indexIn(text) >= 0) {
            bool ok;
            m_sourceFilter = regex.cap(1).toUInt(&ok, 16);
            if (!ok) m_sourceFilter = 255;
        } else {
            m_sourceFilter = 255;
        }
    }
    
    updateStatusLabel();
}

void PGNLogDialog::onDestinationFilterChanged()
{
    if (!m_destinationFilterActive) return;
    
    QString text = m_destinationFilterCombo->currentText();
    if (text == "Any") {
        m_destinationFilter = 255;
    } else if (text.contains("Broadcast")) {
        m_destinationFilter = 255;
    } else {
        // Extract address from text (format: "0xXX (nnn)" or "Device Name (0xXX)")
        QRegExp regex("0x([0-9A-F]{2})");
        if (regex.indexIn(text) >= 0) {
            bool ok;
            m_destinationFilter = regex.cap(1).toUInt(&ok, 16);
            if (!ok) m_destinationFilter = 255;
        } else {
            m_destinationFilter = 255;
        }
    }
    
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
    } else {
        m_destinationFilter = 255; // No filter
    }
    
    updateStatusLabel();
}

void PGNLogDialog::onClearFilters()
{
    m_sourceFilterEnabled->setChecked(false);
    m_destinationFilterEnabled->setChecked(false);
    m_sourceFilterCombo->setCurrentIndex(0); // "Any"
    m_destinationFilterCombo->setCurrentIndex(0); // "Any"
    
    m_sourceFilterActive = false;
    m_destinationFilterActive = false;
    m_sourceFilter = 255;
    m_destinationFilter = 255;
    
    updateStatusLabel();
}

void PGNLogDialog::updateStatusLabel()
{
    QString status = "Live NMEA2000 PGN message log";
    
    QStringList activeFilters;
    if (m_sourceFilterActive && m_sourceFilter != 255) {
        activeFilters << QString("Source: 0x%1").arg(m_sourceFilter, 2, 16, QChar('0')).toUpper();
    }
    if (m_destinationFilterActive && m_destinationFilter != 255) {
        activeFilters << QString("Destination: 0x%1").arg(m_destinationFilter, 2, 16, QChar('0')).toUpper();
    }
    
    if (!activeFilters.isEmpty()) {
        status += " - Filtered by " + activeFilters.join(", ");
    } else {
        status += " - Real-time updates";
    }
    
    m_statusLabel->setText(status);
}

bool PGNLogDialog::messagePassesFilter(const tN2kMsg& msg)
{
    // Check source filter
    if (m_sourceFilterActive && m_sourceFilter != 255) {
        if (msg.Source != m_sourceFilter) {
            return false;
        }
    }
    
    // Check destination filter
    if (m_destinationFilterActive && m_destinationFilter != 255) {
        if (msg.Destination != m_destinationFilter) {
            return false;
        }
    }
    
    return true;
}

#include "pgnlogdialog.h"
#include <QMessageBox>

PGNLogDialog::PGNLogDialog(QWidget *parent)
    : QDialog(parent)
    , m_logTextEdit(nullptr)
    , m_clearButton(nullptr)
    , m_closeButton(nullptr)
    , m_statusLabel(nullptr)
    , m_sourceFilter(255)  // No filter initially
    , m_filterEnabled(false)
{
    setupUI();
    
    setWindowTitle("NMEA2000 PGN Message Log");
    setModal(false);
    resize(800, 600);
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
    // Apply source filter if enabled
    if (m_filterEnabled && msg.Source != m_sourceFilter) {
        return; // Skip this message
    }
    
    QString pgnInfo = QString("PGN: %1, Priority: %2, Source: 0x%3, Destination: %4")
                          .arg(msg.PGN)
                          .arg(msg.Priority)
                          .arg(msg.Source, 2, 16, QChar('0')).toUpper()
                          .arg(msg.Destination);

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
    m_filterEnabled = true;
    
    // Update status label to show filter
    m_statusLabel->setText(QString("PGN Log - Filtered for Device 0x%1")
                          .arg(sourceAddress, 2, 16, QChar('0')).toUpper());
    
    // Clear existing log and add new header
    clearLog();
    m_logTextEdit->append(QString("Filtering messages from device 0x%1")
                         .arg(sourceAddress, 2, 16, QChar('0')).toUpper());
    m_logTextEdit->append("===========================================");
}

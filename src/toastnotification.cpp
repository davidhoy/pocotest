#include "toastnotification.h"
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#ifndef WASM_BUILD
#include <QScreen>
#endif

ToastNotification::ToastNotification(const QString& message, ToastType type, int durationMs, QWidget* parent)
    : QFrame(parent)
    , m_messageLabel(nullptr)
    , m_closeButton(nullptr)
    , m_displayTimer(new QTimer(this))
    , m_opacityEffect(new QGraphicsOpacityEffect(this))
    , m_fadeAnimation(new QPropertyAnimation(m_opacityEffect, "opacity", this))
    , m_message(message)
    , m_type(type)
    , m_durationMs(durationMs)
    , m_isHovered(false)
    , m_isClosing(false)
{
    setupUI();
    setupAnimation();
    
    // Set initial opacity
    m_opacityEffect->setOpacity(1.0);
    setGraphicsEffect(m_opacityEffect);
    
    // Configure display timer
    m_displayTimer->setSingleShot(true);
    connect(m_displayTimer, &QTimer::timeout, this, &ToastNotification::startFadeOut);
    
    // Set window flags for popup behavior
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    
    setMessage(message);
    setType(type);
}

void ToastNotification::setupUI()
{
    setFixedSize(350, 80);
    setFrameStyle(QFrame::NoFrame);
    
    // Main layout
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(15, 12, 15, 12);
    mainLayout->setSpacing(12);
    
    // Message label
    m_messageLabel = new QLabel();
    m_messageLabel->setWordWrap(true);
    m_messageLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_messageLabel->setStyleSheet("color: white; font-size: 13px; font-weight: 500;");
    mainLayout->addWidget(m_messageLabel, 1);
    
    // Close button
    m_closeButton = new QPushButton("✕");
    m_closeButton->setFixedSize(20, 20);
    m_closeButton->setStyleSheet(
        "QPushButton {"
        "    background: transparent;"
        "    border: none;"
        "    color: white;"
        "    font-size: 14px;"
        "    font-weight: bold;"
        "    border-radius: 10px;"
        "}"
        "QPushButton:hover {"
        "    background: rgba(255, 255, 255, 0.2);"
        "}"
        "QPushButton:pressed {"
        "    background: rgba(255, 255, 255, 0.3);"
        "}"
    );
    connect(m_closeButton, &QPushButton::clicked, this, &ToastNotification::onCloseButtonClicked);
    mainLayout->addWidget(m_closeButton);
    
    // Set mouse tracking for hover effects
    setMouseTracking(true);
}

void ToastNotification::setupAnimation()
{
    m_fadeAnimation->setDuration(500);
    m_fadeAnimation->setStartValue(1.0);
    m_fadeAnimation->setEndValue(0.0);
    
    connect(m_fadeAnimation, &QPropertyAnimation::finished, this, &ToastNotification::onFadeOutFinished);
}

void ToastNotification::show()
{
    // Position the toast in the top-right corner of the screen
    if (parent()) {
        // Position relative to parent widget
        QWidget* parentWidget = qobject_cast<QWidget*>(parent());
        if (parentWidget) {
#ifdef WASM_BUILD
            // WASM: Use simpler positioning for browser compatibility
            QPoint parentPos = parentWidget->pos();
            QSize parentSize = parentWidget->size();
            
            // Try to get window-relative position
            QWidget* topLevel = parentWidget->window();
            if (topLevel && topLevel != parentWidget) {
                QPoint topLevelPos = topLevel->pos();
                QPoint relativePos = parentWidget->mapTo(topLevel, QPoint(0, 0));
                parentPos = topLevelPos + relativePos;
            }
#else
            QPoint parentPos = parentWidget->mapToGlobal(QPoint(0, 0));
            QSize parentSize = parentWidget->size();
#endif
            
            int x = parentPos.x() + parentSize.width() - width() - 20;
            int y = parentPos.y() + 20;
            
            move(x, y);
        }
    } else {
        // Position relative to screen
#ifdef WASM_BUILD
        // WASM: Use main window instead of screen API
        QWidget* mainWindow = QApplication::activeWindow();
        if (!mainWindow) {
            mainWindow = QApplication::topLevelWidgets().isEmpty() ? 
                         nullptr : QApplication::topLevelWidgets().first();
        }
        
        QRect screenGeometry;
        if (mainWindow) {
            screenGeometry = mainWindow->geometry();
        } else {
            // Fallback to reasonable default for WASM
            screenGeometry = QRect(0, 0, 1024, 768);
        }
#else
        QScreen* screen = QApplication::primaryScreen();
        QRect screenGeometry = screen->availableGeometry();
#endif
        
        int x = screenGeometry.right() - width() - 20;
        int y = screenGeometry.top() + 20;
        
        move(x, y);
    }
    
    QFrame::show();
    raise();
    activateWindow();
    
    // Start the display timer
    if (m_durationMs > 0) {
        m_displayTimer->start(m_durationMs);
    }
}

void ToastNotification::setMessage(const QString& message)
{
    m_message = message;
    if (m_messageLabel) {
        m_messageLabel->setText(message);
    }
}

void ToastNotification::setType(ToastType type)
{
    m_type = type;
    updateStyleForType();
}

void ToastNotification::setDuration(int durationMs)
{
    m_durationMs = durationMs;
}

void ToastNotification::startFadeOut()
{
    if (m_isClosing || m_isHovered) {
        return; // Don't fade out if user is hovering or already closing
    }
    
    m_isClosing = true;
    emit aboutToClose();
    m_fadeAnimation->start();
}

void ToastNotification::hide()
{
    if (m_isClosing) {
        return;
    }
    
    m_isClosing = true;
    m_displayTimer->stop();
    emit aboutToClose();
    m_fadeAnimation->start();
}

void ToastNotification::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw rounded rectangle background
    QPainterPath path;
    path.addRoundedRect(rect(), 8, 8);
    
    // Get color for current type
    QColor bgColor = getTypeColor();
    
    // Add subtle gradient
    QLinearGradient gradient(0, 0, 0, height());
    gradient.setColorAt(0, bgColor);
    gradient.setColorAt(1, bgColor.darker(110));
    
    painter.fillPath(path, gradient);
    
    // Draw subtle border
    painter.setPen(QPen(bgColor.lighter(120), 1));
    painter.drawPath(path);
}

void ToastNotification::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        hide();
    }
    QFrame::mousePressEvent(event);
}

void ToastNotification::enterEvent(QEnterEvent* event)
{
    m_isHovered = true;
    m_displayTimer->stop(); // Pause auto-close when hovering
    QFrame::enterEvent(event);
}

void ToastNotification::leaveEvent(QEvent* event)
{
    m_isHovered = false;
    
    // Resume auto-close timer if not already closing
    if (!m_isClosing && m_durationMs > 0) {
        m_displayTimer->start(1000); // Give 1 second before auto-close resumes
    }
    
    QFrame::leaveEvent(event);
}

void ToastNotification::onFadeOutFinished()
{
    emit closed();
    close();
}

void ToastNotification::onCloseButtonClicked()
{
    hide();
}

void ToastNotification::updateStyleForType()
{
    update(); // Trigger repaint with new colors
}

QString ToastNotification::getTypeIcon() const
{
    switch (m_type) {
        case Success: return "✓";
        case Warning: return "⚠";
        case Error: return "✗";
        case Info:
        default: return "ℹ";
    }
}

QColor ToastNotification::getTypeColor() const
{
    switch (m_type) {
        case Success: return QColor(46, 125, 50);   // Green
        case Warning: return QColor(255, 152, 0);   // Orange
        case Error: return QColor(211, 47, 47);     // Red
        case Info:
        default: return QColor(25, 118, 210);       // Blue
    }
}
#include "toastmanager.h"
#include <QApplication>
#ifndef WASM_BUILD
#include <QScreen>
#endif
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QMap>
#include <QPointer>
#include <QDebug>

ToastManager* ToastManager::m_instance = nullptr;

ToastManager::ToastManager(QObject* parent)
    : QObject(parent)
    , m_maxToasts(5)
    , m_repositionTimer(new QTimer(this))
    , m_toastSpacing(10)
    , m_positionMode(WindowRelative)  // Default to window-relative positioning
{
    m_repositionTimer->setSingleShot(true);
    m_repositionTimer->setInterval(50);
    connect(m_repositionTimer, &QTimer::timeout, this, &ToastManager::updateToastPositions);
}

ToastManager::~ToastManager()
{
    clearAllToasts();
}

ToastManager* ToastManager::instance()
{
    if (!m_instance) {
        m_instance = new ToastManager();
    }
    return m_instance;
}

void ToastManager::showToast(const QString& message, ToastNotification::ToastType type, 
                            int durationMs, QWidget* parent)
{
    ToastNotification* toast = new ToastNotification(message, type, durationMs, parent);
    addToast(toast);
    toast->show();
}

void ToastManager::showInfo(const QString& message, QWidget* parent)
{
    showToast(message, ToastNotification::Info, 3000, parent);
}

void ToastManager::showSuccess(const QString& message, QWidget* parent)
{
    showToast(message, ToastNotification::Success, 3000, parent);
}

void ToastManager::showWarning(const QString& message, QWidget* parent)
{
    showToast(message, ToastNotification::Warning, 4000, parent);
}

void ToastManager::showError(const QString& message, QWidget* parent)
{
    showToast(message, ToastNotification::Error, 5000, parent);
}

void ToastManager::clearAllToasts()
{
    // Create a copy of the list since hiding toasts will modify m_activeToasts
    QList<ToastNotification*> toastsToClose = m_activeToasts;
    
    for (ToastNotification* toast : toastsToClose) {
        if (toast) {
            toast->hide();
        }
    }
    
    m_activeToasts.clear();
}

void ToastManager::setMaxToasts(int maxToasts)
{
    m_maxToasts = qMax(1, maxToasts);
    
    // Remove excess toasts if necessary
    while (m_activeToasts.size() > m_maxToasts) {
        removeOldestToast();
    }
}

int ToastManager::getMaxToasts() const
{
    return m_maxToasts;
}

void ToastManager::setPositionMode(PositionMode mode)
{
    m_positionMode = mode;
    repositionToasts(); // Update positions immediately
}

ToastManager::PositionMode ToastManager::getPositionMode() const
{
    return m_positionMode;
}

void ToastManager::onToastClosed()
{
    ToastNotification* toast = qobject_cast<ToastNotification*>(sender());
    if (toast) {
        m_activeToasts.removeAll(toast);
        repositionToasts();
    }
}

void ToastManager::repositionToasts()
{
    m_repositionTimer->start();
}

void ToastManager::addToast(ToastNotification* toast)
{
    if (!toast) return;
    
    // Remove oldest toast if we're at the limit
    if (m_activeToasts.size() >= m_maxToasts) {
        removeOldestToast();
    }
    
    // Connect to the toast's closed signal
    connect(toast, &ToastNotification::closed, this, &ToastManager::onToastClosed);
    
    // Also connect to destroyed signal as a safety net
    connect(toast, &QObject::destroyed, this, [this](QObject* obj) {
        // Remove the destroyed toast from active list
        m_activeToasts.removeAll(static_cast<ToastNotification*>(obj));
    });
    
    // Add to active toasts
    m_activeToasts.append(toast);
    
    // Update positions after a short delay to allow the toast to be shown
    QTimer::singleShot(10, this, &ToastManager::updateToastPositions);
}

void ToastManager::removeOldestToast()
{
    if (!m_activeToasts.isEmpty()) {
        ToastNotification* oldestToast = m_activeToasts.first();
        if (oldestToast) {
            oldestToast->hide();
        }
    }
}

void ToastManager::updateToastPositions()
{
    if (m_activeToasts.isEmpty()) return;
    
    if (m_positionMode == ScreenRelative) {
        // Position all toasts relative to screen (original behavior)
#ifdef WASM_BUILD
        // WASM: Use application's main window instead of screen for better browser compatibility
        QWidget* mainWindow = QApplication::activeWindow();
        if (!mainWindow) {
            mainWindow = QApplication::topLevelWidgets().isEmpty() ? 
                         nullptr : QApplication::topLevelWidgets().first();
        }
        
        QRect screenGeometry;
        if (mainWindow) {
            screenGeometry = mainWindow->geometry();
        } else {
            // Fallback to a reasonable default size for WASM
            screenGeometry = QRect(0, 0, 1024, 768);
        }
#else
        QScreen* screen = QApplication::primaryScreen();
        QRect screenGeometry = screen->availableGeometry();
#endif
        
        int startX = screenGeometry.right() - 20;
        int currentY = screenGeometry.top() + 20;
        
        for (int i = 0; i < m_activeToasts.size(); ++i) {
            ToastNotification* toast = m_activeToasts[i];
            if (!toast || !toast->isVisible()) continue;
            
            int toastX = startX - toast->width();
            int toastY = currentY;
            
            QPoint newPos(toastX, toastY);
            if (toast->pos() != newPos) {
                QPropertyAnimation* moveAnimation = new QPropertyAnimation(toast, "pos");
                moveAnimation->setDuration(300);
                moveAnimation->setStartValue(toast->pos());
                moveAnimation->setEndValue(newPos);
                moveAnimation->setEasingCurve(QEasingCurve::OutCubic);
                
                connect(moveAnimation, &QPropertyAnimation::finished, moveAnimation, &QPropertyAnimation::deleteLater);
                moveAnimation->start();
            }
            
            currentY += toast->height() + m_toastSpacing;
        }
    } else {
        // Position toasts relative to their parent windows
        // Group toasts by parent - with defensive programming
        QMap<QWidget*, QList<ToastNotification*>> toastGroups;
        
        // Make a copy of the list to avoid modification during iteration
        QList<ToastNotification*> toastsCopy = m_activeToasts;
        
        for (ToastNotification* toast : toastsCopy) {
            // Skip null pointers
            if (!toast) {
                continue;
            }
            
            // Verify toast is still in our active list (may have been removed by destroyed signal)
            if (!m_activeToasts.contains(toast)) {
                continue;
            }
            
            // Only proceed if toast is visible
            if (!toast->isVisible()) {
                continue;
            }
            
            QWidget* parent = qobject_cast<QWidget*>(toast->parent());
            toastGroups[parent].append(toast);
        }
        
        // Position each group relative to its parent
        for (auto it = toastGroups.begin(); it != toastGroups.end(); ++it) {
            updateToastPositionsForParent(it.key());
        }
    }
}

void ToastManager::updateToastPositionsForParent(QWidget* parent)
{
    // Get all toasts for this parent
    QList<ToastNotification*> parentToasts;
    for (ToastNotification* toast : m_activeToasts) {
        if (toast && toast->isVisible() && toast->parent() == parent) {
            parentToasts.append(toast);
        }
    }
    
    if (parentToasts.isEmpty()) return;
    
    // Get parent geometry
    QRect parentGeometry = getParentGeometry(parent);
    
    // Calculate starting position (top-right corner of parent)
    int startX = parentGeometry.right() - 20;  // 20px margin from right edge
    int currentY = parentGeometry.top() + 20;  // 20px margin from top
    
    // Position each toast from top to bottom
    for (ToastNotification* toast : parentToasts) {
        int toastX = startX - toast->width();
        int toastY = currentY;
        
        QPoint newPos(toastX, toastY);
        if (toast->pos() != newPos) {
            QPropertyAnimation* moveAnimation = new QPropertyAnimation(toast, "pos");
            moveAnimation->setDuration(300);
            moveAnimation->setStartValue(toast->pos());
            moveAnimation->setEndValue(newPos);
            moveAnimation->setEasingCurve(QEasingCurve::OutCubic);
            
            connect(moveAnimation, &QPropertyAnimation::finished, moveAnimation, &QPropertyAnimation::deleteLater);
            moveAnimation->start();
        }
        
        currentY += toast->height() + m_toastSpacing;
    }
}

QRect ToastManager::getParentGeometry(QWidget* parent) const
{
    if (parent) {
#ifdef WASM_BUILD
        // WASM: Use widget geometry directly for better browser compatibility
        // mapToGlobal() may have limited functionality in browser sandbox
        QPoint parentPos = parent->pos();
        QSize parentSize = parent->size();
        
        // Try to get global position, but fall back to relative positioning if needed
        QWidget* topLevel = parent->window();
        if (topLevel && topLevel != parent) {
            QPoint topLevelPos = topLevel->pos();
            QPoint relativePos = parent->mapTo(topLevel, QPoint(0, 0));
            parentPos = topLevelPos + relativePos;
        }
        
        return QRect(parentPos, parentSize);
#else
        // Get the global geometry of the parent widget
        QPoint globalPos = parent->mapToGlobal(QPoint(0, 0));
        return QRect(globalPos, parent->size());
#endif
    } else {
        // No parent - fall back to screen geometry
#ifdef WASM_BUILD
        // WASM: Use main window geometry instead of screen
        QWidget* mainWindow = QApplication::activeWindow();
        if (!mainWindow) {
            mainWindow = QApplication::topLevelWidgets().isEmpty() ? 
                         nullptr : QApplication::topLevelWidgets().first();
        }
        
        if (mainWindow) {
            return mainWindow->geometry();
        } else {
            // Fallback to reasonable default for WASM
            return QRect(0, 0, 1024, 768);
        }
#else
        QScreen* screen = QApplication::primaryScreen();
        return screen->availableGeometry();
#endif
    }
}
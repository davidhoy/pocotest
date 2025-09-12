#ifndef TOASTMANAGER_H
#define TOASTMANAGER_H

#include <QObject>
#include <QList>
#include <QTimer>
#include <QWidget>
#include "toastnotification.h"

class ToastManager : public QObject
{
    Q_OBJECT

public:
    enum PositionMode {
        ScreenRelative,     // Position relative to screen (default)
        WindowRelative      // Position relative to parent window
    };

    static ToastManager* instance();
    
    void showToast(const QString& message, ToastNotification::ToastType type = ToastNotification::Info, 
                   int durationMs = 3000, QWidget* parent = nullptr);
    
    void showInfo(const QString& message, QWidget* parent = nullptr);
    void showSuccess(const QString& message, QWidget* parent = nullptr);
    void showWarning(const QString& message, QWidget* parent = nullptr);
    void showError(const QString& message, QWidget* parent = nullptr);
    
    void clearAllToasts();
    void setMaxToasts(int maxToasts);
    int getMaxToasts() const;
    
    void setPositionMode(PositionMode mode);
    PositionMode getPositionMode() const;

private slots:
    void onToastClosed();
    void repositionToasts();

private:
    explicit ToastManager(QObject* parent = nullptr);
    ~ToastManager();
    
    void addToast(ToastNotification* toast);
    void removeOldestToast();
    void updateToastPositions();
    void updateToastPositionsForParent(QWidget* parent);
    QRect getParentGeometry(QWidget* parent) const;
    
    static ToastManager* m_instance;
    QList<ToastNotification*> m_activeToasts;
    int m_maxToasts;
    QTimer* m_repositionTimer;
    int m_toastSpacing;
    PositionMode m_positionMode;
};

// Convenience macros for global toast notifications
#define TOAST_INFO(message) ToastManager::instance()->showInfo(message)
#define TOAST_SUCCESS(message) ToastManager::instance()->showSuccess(message)
#define TOAST_WARNING(message) ToastManager::instance()->showWarning(message)
#define TOAST_ERROR(message) ToastManager::instance()->showError(message)

#endif // TOASTMANAGER_H
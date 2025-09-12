#ifndef TOASTNOTIFICATION_H
#define TOASTNOTIFICATION_H

#include <QFrame>
#include <QLabel>
#include <QTimer>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMouseEvent>
#include <QEnterEvent>

class ToastNotification : public QFrame
{
    Q_OBJECT

public:
    enum ToastType {
        Info,
        Success,
        Warning,
        Error
    };

    explicit ToastNotification(const QString& message, ToastType type = Info, int durationMs = 3000, QWidget* parent = nullptr);
    
    void show();
    void setMessage(const QString& message);
    void setType(ToastType type);
    void setDuration(int durationMs);

signals:
    void aboutToClose();
    void closed();

public slots:
    void startFadeOut();
    void hide();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private slots:
    void onFadeOutFinished();
    void onCloseButtonClicked();

private:
    void setupUI();
    void setupAnimation();
    void updateStyleForType();
    QString getTypeIcon() const;
    QColor getTypeColor() const;
    
    QLabel* m_messageLabel;
    QPushButton* m_closeButton;
    QTimer* m_displayTimer;
    QGraphicsOpacityEffect* m_opacityEffect;
    QPropertyAnimation* m_fadeAnimation;
    
    QString m_message;
    ToastType m_type;
    int m_durationMs;
    bool m_isHovered;
    bool m_isClosing;
};

#endif // TOASTNOTIFICATION_H
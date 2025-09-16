#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QColor>
#include <QPalette>

class ThemeManager : public QObject
{
    Q_OBJECT

public:
    enum Theme {
        Light,
        Dark,
        Auto  // Follow system theme
    };

    static ThemeManager* instance();
    
    // Theme detection
    bool isDarkTheme() const;
    Theme currentTheme() const { return m_currentTheme; }
    void setTheme(Theme theme);
    
    // Color getters that adapt to current theme
    QColor textColor() const;
    QColor backgroundColor() const;
    QColor borderColor() const;
    QColor selectionColor() const;
    QColor alternateBackgroundColor() const;
    QColor headerBackgroundColor() const;
    
    // Status colors
    QColor successColor() const;
    QColor warningColor() const;
    QColor errorColor() const;
    
    // Get themed status styles
    QString getStatusStyle() const;
    QString getSuccessStatusStyle() const;
    QString getWarningStatusStyle() const;
    QString getErrorStatusStyle() const;
    
    // Get an adaptive palette
    QPalette adaptivePalette() const;

signals:
    void themeChanged();

private slots:
    void detectSystemTheme();

private:
    explicit ThemeManager(QObject *parent = nullptr);
    void updateColors();

    Theme m_currentTheme;
    bool m_isDark;
    
    // Color members
    QColor m_textColor;
    QColor m_backgroundColor;
    QColor m_borderColor;
    QColor m_selectionColor;
    QColor m_alternateBackgroundColor;
    QColor m_headerBackgroundColor;
    
    // Status colors
    QColor m_successColor;
    QColor m_warningColor;
    QColor m_errorColor;
    QColor m_successBackgroundColor;
    QColor m_warningBackgroundColor;
    QColor m_errorBackgroundColor;
};

#endif // THEMEMANAGER_H
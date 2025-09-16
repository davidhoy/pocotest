#include "thememanager.h"
#include <QApplication>
#include <QStyleHints>
#include <QPalette>
#include <QColor>
#include <QDebug>
#include <QObject>
#include <QTimer>
#include <QStyle>

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
    , m_currentTheme(Auto)
    , m_isDark(false)
{
    detectSystemTheme();
    
    // Ensure colors are initialized even if theme doesn't change
    updateColors();
    
    // Connect to application palette changes to detect system theme changes
    connect(QApplication::instance(), SIGNAL(paletteChanged(const QPalette&)),
            this, SLOT(detectSystemTheme()));
}

ThemeManager* ThemeManager::instance()
{
    static ThemeManager* inst = new ThemeManager();
    return inst;
}

void ThemeManager::detectSystemTheme()
{
    // Use palette to detect dark mode
    QPalette palette = QApplication::palette();
    QColor windowColor = palette.color(QPalette::Window);
    QColor textColor = palette.color(QPalette::WindowText);
    
    // Debug output
    qDebug() << "Theme detection - Window color:" << windowColor.name() 
             << "lightness:" << windowColor.lightness();
    qDebug() << "Theme detection - Text color:" << textColor.name() 
             << "lightness:" << textColor.lightness();
    
    // Simple heuristic: if window is darker than text, it's probably dark mode
    bool newIsDark = windowColor.lightness() < textColor.lightness();
    
    qDebug() << "Theme detection result:" << (newIsDark ? "DARK" : "LIGHT");
    
    if (m_isDark != newIsDark) {
        m_isDark = newIsDark;
        updateColors();
        qDebug() << "Theme changed to:" << (m_isDark ? "Dark" : "Light");
        emit themeChanged();
    }
}

void ThemeManager::setTheme(Theme theme)
{
    if (m_currentTheme != theme) {
        m_currentTheme = theme;
        
        if (theme == Auto) {
            detectSystemTheme();
        } else {
            m_isDark = (theme == Dark);
            updateColors();
            emit themeChanged();
        }
    }
}

bool ThemeManager::isDarkTheme() const
{
    if (m_currentTheme == Auto) {
        return m_isDark;
    }
    return m_currentTheme == Dark;
}

void ThemeManager::updateColors()
{
    qDebug() << "updateColors() called, isDarkTheme():" << isDarkTheme();
    
    if (isDarkTheme()) {
        m_textColor = QColor(240, 240, 240);           // Light gray text
        m_backgroundColor = QColor(45, 45, 45);        // Dark gray background
        m_borderColor = QColor(80, 80, 80);            // Medium gray borders
        m_selectionColor = QColor(70, 140, 200);       // Blue selection
        m_alternateBackgroundColor = QColor(55, 55, 55); // Slightly lighter for alternating rows
        m_headerBackgroundColor = QColor(60, 60, 60);  // Header background
        
        // Status colors for dark theme
        m_successColor = QColor(76, 175, 80);          // Green text
        m_warningColor = QColor(255, 152, 0);          // Orange text
        m_errorColor = QColor(244, 67, 54);            // Red text
        m_successBackgroundColor = QColor(20, 40, 20); // Dark green background
        m_warningBackgroundColor = QColor(40, 30, 10); // Dark orange background
        m_errorBackgroundColor = QColor(40, 20, 20);   // Dark red background
    } else {
        m_textColor = QColor(0, 0, 0);                 // Black text
        m_backgroundColor = QColor(255, 255, 255);     // White background
        m_borderColor = QColor(200, 200, 200);         // Light gray borders
        m_selectionColor = QColor(100, 150, 200);      // Light blue selection
        m_alternateBackgroundColor = QColor(245, 245, 245); // Very light gray for alternating rows
        m_headerBackgroundColor = QColor(240, 240, 240); // Light header background
        
        // Status colors for light theme
        m_successColor = QColor(46, 125, 50);          // Dark green text
        m_warningColor = QColor(245, 124, 0);          // Dark orange text
        m_errorColor = QColor(211, 47, 47);            // Dark red text
        m_successBackgroundColor = QColor(230, 255, 230); // Light green background
        m_warningBackgroundColor = QColor(255, 243, 205); // Light orange background
        m_errorBackgroundColor = QColor(255, 230, 230);   // Light red background
    }
    
    qDebug() << "Colors set - Text:" << m_textColor.name() 
             << "Background:" << m_backgroundColor.name();
}

QColor ThemeManager::textColor() const
{
    return m_textColor;
}

QColor ThemeManager::backgroundColor() const
{
    return m_backgroundColor;
}

QColor ThemeManager::borderColor() const
{
    return m_borderColor;
}

QColor ThemeManager::selectionColor() const
{
    return m_selectionColor;
}

QColor ThemeManager::alternateBackgroundColor() const
{
    return m_alternateBackgroundColor;
}

QColor ThemeManager::headerBackgroundColor() const
{
    return m_headerBackgroundColor;
}

QColor ThemeManager::successColor() const
{
    return m_successColor;
}

QColor ThemeManager::warningColor() const
{
    return m_warningColor;
}

QColor ThemeManager::errorColor() const
{
    return m_errorColor;
}

QString ThemeManager::getStatusStyle() const
{
    return QString("font-weight: bold; color: %1; background-color: %2; padding: 5px; border: 1px solid %3; border-radius: 3px;")
           .arg(m_textColor.name())
           .arg(m_backgroundColor.name())
           .arg(m_borderColor.name());
}

QString ThemeManager::getSuccessStatusStyle() const
{
    return QString("font-weight: bold; color: %1; background-color: %2; padding: 5px; border: 1px solid %3; border-radius: 3px;")
           .arg(m_successColor.name())
           .arg(m_successBackgroundColor.name())
           .arg(m_successColor.darker(150).name());
}

QString ThemeManager::getWarningStatusStyle() const
{
    return QString("font-weight: bold; color: %1; background-color: %2; padding: 5px; border: 1px solid %3; border-radius: 3px;")
           .arg(m_warningColor.name())
           .arg(m_warningBackgroundColor.name())
           .arg(m_warningColor.darker(150).name());
}

QString ThemeManager::getErrorStatusStyle() const
{
    return QString("font-weight: bold; color: %1; background-color: %2; padding: 5px; border: 1px solid %3; border-radius: 3px;")
           .arg(m_errorColor.name())
           .arg(m_errorBackgroundColor.name())
           .arg(m_errorColor.darker(150).name());
}

QPalette ThemeManager::adaptivePalette() const
{
    QPalette palette = QApplication::palette();
    
    palette.setColor(QPalette::Window, m_backgroundColor);
    palette.setColor(QPalette::WindowText, m_textColor);
    palette.setColor(QPalette::Base, m_backgroundColor);
    palette.setColor(QPalette::AlternateBase, m_alternateBackgroundColor);
    palette.setColor(QPalette::Text, m_textColor);
    palette.setColor(QPalette::Button, m_headerBackgroundColor);
    palette.setColor(QPalette::ButtonText, m_textColor);
    palette.setColor(QPalette::Highlight, m_selectionColor);
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    
    return palette;
}
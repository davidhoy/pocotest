#ifndef PGNLOGDIALOG_H
#define PGNLOGDIALOG_H

#include <QDialog>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <N2kMsg.h>

class PGNLogDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PGNLogDialog(QWidget *parent = nullptr);
    ~PGNLogDialog();
    
    void appendMessage(const tN2kMsg& msg);
    void setSourceFilter(uint8_t sourceAddress);
    void updateDeviceList(const QStringList& devices);

private slots:
    void clearLog();
    void onCloseClicked();
    void onSourceFilterChanged();
    void onDestinationFilterChanged();
    void onSourceFilterEnabled(bool enabled);
    void onDestinationFilterEnabled(bool enabled);
    void onClearFilters();

private:
    void setupUI();
    void updateStatusLabel();
    bool messagePassesFilter(const tN2kMsg& msg);

private:
    QTextEdit* m_logTextEdit;
    QPushButton* m_clearButton;
    QPushButton* m_closeButton;
    QPushButton* m_clearFiltersButton;
    QLabel* m_statusLabel;
    
    // Filter controls
    QGroupBox* m_filterGroup;
    QCheckBox* m_sourceFilterEnabled;
    QCheckBox* m_destinationFilterEnabled;
    QComboBox* m_sourceFilterCombo;
    QComboBox* m_destinationFilterCombo;
    
    // Filter state
    uint8_t m_sourceFilter;      // 255 means no filter
    uint8_t m_destinationFilter; // 255 means no filter
    bool m_sourceFilterActive;
    bool m_destinationFilterActive;
};

#endif // PGNLOGDIALOG_H

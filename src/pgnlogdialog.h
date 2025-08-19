#ifndef PGNLOGDIALOG_H
#define PGNLOGDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QDateTime>
#include <QVector>
#include <N2kMsg.h>
#include "dbcdecoder.h"

class PGNLogDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PGNLogDialog(QWidget *parent = nullptr);
    ~PGNLogDialog();
    
    void appendMessage(const tN2kMsg& msg);
    void appendSentMessage(const tN2kMsg& msg); // For messages sent by this application
    void setSourceFilter(uint8_t sourceAddress);
    void setDestinationFilter(uint8_t destinationAddress);
    void setFilterLogic(bool useOrLogic); // true for OR, false for AND
    void updateDeviceList(const QStringList& devices);
    void clearAllFilters(); // Clear all filters and reset to default view

private slots:
    void clearLog();
    void onCloseClicked();
    void onSaveLogClicked();
    void onSourceFilterChanged();
    void onDestinationFilterChanged();
    void onSourceFilterEnabled(bool enabled);
    void onDestinationFilterEnabled(bool enabled);
    void onClearFilters();
    void onFilterLogicChanged();
    void onToggleDecoding(bool enabled);
    void onTableItemClicked(int row, int column);
    void onPauseClicked();
    void onStartClicked();
    void onStopClicked();

public:
    enum TimestampMode {
        Absolute,
        Relative
    };
    void setTimestampMode(TimestampMode mode);
    TimestampMode getTimestampMode() const;

private:
    TimestampMode m_timestampMode = Absolute;
    QDateTime m_lastTimestamp;
    QVector<QDateTime> m_messageTimestamps;
    QCheckBox* m_timestampModeCheck = nullptr; // Absolute/Relative toggle
    void setupUI();
    void updateStatusLabel();
    bool messagePassesFilter(const tN2kMsg& msg);

private:
    QTableWidget* m_logTable;
    QPushButton* m_clearButton;
    QPushButton* m_closeButton;
    QPushButton* m_saveButton;
    QPushButton* m_clearFiltersButton;
    QPushButton* m_pauseButton;
    QPushButton* m_startButton;
    QPushButton* m_stopButton;
    QLabel* m_statusLabel;
    
    // Filter controls
    QGroupBox* m_filterGroup;
    QCheckBox* m_sourceFilterEnabled;
    QCheckBox* m_destinationFilterEnabled;
    QComboBox* m_sourceFilterCombo;
    QComboBox* m_destinationFilterCombo;
    QComboBox* m_filterLogicCombo;  // AND/OR logic selector
    QCheckBox* m_decodingEnabled;   // Toggle for DBC decoding
    
    // Filter state
    uint8_t m_sourceFilter;      // 255 means no filter
    uint8_t m_destinationFilter; // Actual destination address to filter for
    bool m_sourceFilterActive;
    bool m_destinationFilterActive;
    bool m_useAndLogic;          // true = AND, false = OR
    
    // Log control state
    bool m_logPaused;
    bool m_logStopped;
    
    // Original DBC Decoder - stable and fast
    DBCDecoder* m_dbcDecoder;
};

#endif // PGNLOGDIALOG_H

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
#include <QDateTime>
#include <QVector>
#include <QLineEdit>
#include <QListWidget>
#include <QGroupBox>
#include <QSet>
#include <QMenu>
#include <QSettings>
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
    void clearLogForLoad(); // Clear log without restarting live logging
    void onCloseClicked();
    void onSaveLogClicked();
    void onLoadLogClicked();
    void onSourceFilterChanged();
    void onDestinationFilterChanged();
    void onClearFilters();
    void onFilterLogicChanged();
    void onToggleDecoding(bool enabled);
    void onTableItemClicked(int row, int column);
    void onPauseClicked();
    void onStartClicked();
    void onStopClicked();
    void onAddPgnIgnore();
    void onRemovePgnIgnore();
    void onAddCommonNoisyPgns();
    void onTableContextMenu(const QPoint& position);
    void onPgnFilteringToggled(bool enabled);

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
    void updateWindowTitle();  // Update window title based on current state
    bool messagePassesFilter(const tN2kMsg& msg);
    void addLoadedMessage(const tN2kMsg& msg, const QString& originalTimestamp);
    void refreshTableFilter(); // Re-apply filters to existing table rows
    
    // Format parsing helpers for loading logs
    bool parseOlderFormatLine(const QString& line, tN2kMsg& msg, QString& timestamp);
    bool parseNewerFormatLine(const QString& line, tN2kMsg& msg, QString& timestamp);

private:
    QTableWidget* m_logTable;
    QPushButton* m_clearButton;
    QPushButton* m_closeButton;
    QPushButton* m_saveButton;
    QPushButton* m_loadButton;
    QPushButton* m_clearFiltersButton;
    QPushButton* m_pauseButton;
    QPushButton* m_startButton;
    QPushButton* m_stopButton;
    QLabel* m_statusLabel;
    
    // Filter controls
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
    bool m_showingLoadedLog;  // Track if we're displaying a loaded log file
    QString m_loadedLogFileName; // Name of loaded log file for title display
    
    // Original DBC Decoder - stable and fast
    DBCDecoder* m_dbcDecoder;
    
    // PGN filtering UI elements
    QLineEdit* m_pgnIgnoreEdit;
    QPushButton* m_addPgnIgnoreButton;
    QPushButton* m_removePgnIgnoreButton;
    QListWidget* m_pgnIgnoreList;
    QCheckBox* m_pgnFilteringEnabled;
    
    // PGN filtering state
    QSet<uint32_t> m_ignoredPgns;
    
    // PGN filtering helper methods
    void addPgnToIgnoreList(uint32_t pgn);
    void removePgnFromIgnoreList(uint32_t pgn);
    void setIgnoredPgns(const QSet<uint32_t>& pgns);
    QSet<uint32_t> getIgnoredPgns() const { return m_ignoredPgns; }
    
    // Settings persistence
    void saveSettings();
    void loadSettings();
};

#endif // PGNLOGDIALOG_H

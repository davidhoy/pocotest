#ifndef ACTIONRECORDER_H
#define ACTIONRECORDER_H

#include <QObject>
#include <QVariantMap>
#include <QTimer>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QListWidget>

class DeviceMainWindow;

/**
 * @brief ActionRecorder captures user interactions and converts them to executable scripts
 * 
 * This class provides record/playback functionality by monitoring user actions
 * in the application and generating equivalent JavaScript or JSON test scripts.
 */
class ActionRecorder : public QObject
{
    Q_OBJECT

public:
    explicit ActionRecorder(DeviceMainWindow* mainWindow, QObject *parent = nullptr);
    ~ActionRecorder();
    
    // Recording control
    void startRecording(const QString& testName);
    void stopRecording();
    void pauseRecording();
    void resumeRecording();
    void clearRecording();
    
    // Recording state
    bool isRecording() const { return m_recording && !m_paused; }
    bool isPaused() const { return m_paused; }
    QString currentTestName() const { return m_testName; }
    QString getTestName() const { return m_testName; }
    int actionCount() const { return m_actions.size(); }
    qint64 recordingDuration() const;
    void clearActions() { m_actions.clear(); }
    
    // Script generation
    QString generateJavaScript() const;
    QString generateJsonTest() const;
    QJsonObject generateJsonObject() const;
    
    // Configuration
    void setMinimumWaitTime(int ms) { m_minimumWaitMs = ms; }
    void setMaximumWaitTime(int ms) { m_maximumWaitMs = ms; }
    void setAutoDetectWaits(bool enabled) { m_autoDetectWaits = enabled; }
    void setIncludeTimestamps(bool enabled) { m_includeTimestamps = enabled; }
    
    // Utility methods
    QString formatDuration(qint64 durationMs) const;

public slots:
    // Event capture methods - called by UI components
    void recordPGNSent(int pgn, const QString& data, int destination);
    void recordPGNSentToDevice(int pgn, const QString& data, const QString& deviceAddress);
    void recordFilterChange(const QString& sourceFilter, const QString& destFilter);
    void recordLogClear();
    void recordDeviceEnumeration();
    void recordDeviceSelection(const QString& deviceAddress);
    void recordUserComment(const QString& comment);
    void recordManualWait(int durationMs);
    void recordAssertion(const QString& condition, const QString& description);
    
    // Automatic event detection
    void recordMessageReceived(int pgn, const QString& source);
    void recordDeviceDiscovered(const QString& deviceAddress, const QString& manufacturer);

signals:
    void recordingStarted(const QString& testName);
    void recordingStopped(int actionCount, qint64 durationMs);
    void recordingPaused();
    void recordingResumed();
    void actionRecorded(const QString& description, int actionNumber);
    void scriptGenerated(const QString& script);

private:
    struct RecordedAction {
        enum Type {
            SendPGN,
            SendPGNToDevice,
            SetFilter,
            ClearLog,
            DeviceEnumeration,
            SelectDevice,
            UserComment,
            ManualWait,
            AutoWait,
            Assertion,
            MessageReceived,
            DeviceDiscovered
        };
        
        Type type;
        QVariantMap parameters;
        qint64 timestamp;
        QString description;
        bool userInitiated;
        
        RecordedAction(Type t, const QVariantMap& params, const QString& desc, bool userInit = true)
            : type(t), parameters(params), timestamp(QDateTime::currentMSecsSinceEpoch())
            , description(desc), userInitiated(userInit) {}
    };
    
    DeviceMainWindow* m_mainWindow;
    
    // Recording state
    bool m_recording;
    bool m_paused;
    QString m_testName;
    QList<RecordedAction> m_actions;
    qint64 m_recordingStartTime;
    qint64 m_lastActionTime;
    qint64 m_pausedTime;
    qint64 m_totalPausedDuration;
    
    // Configuration
    int m_minimumWaitMs;
    int m_maximumWaitMs;
    bool m_autoDetectWaits;
    bool m_includeTimestamps;
    
    // Helper methods
    void addAction(RecordedAction::Type type, const QVariantMap& params, 
                  const QString& description, bool userInitiated = true);
    void insertAutoWait();
    QString formatTimestamp(qint64 timestamp) const;
    int calculateWaitTime(qint64 fromTime, qint64 toTime) const;
    QString escapeString(const QString& str) const;
    QString actionTypeToString(RecordedAction::Type type) const;
    QString generateJavaScriptFunction() const;
    QString generateJavaScriptAction(const RecordedAction& action, int index) const;
    QJsonObject generateJsonStep(const RecordedAction& action) const;
    
    // Script formatting helpers
    QString formatPGNData(const QString& hexData) const;
    QString formatDeviceAddress(const QString& address) const;
    QString sanitizeTestName(const QString& name) const;
};

/**
 * @brief RecordingControlWidget provides UI controls for the recording functionality
 */
class RecordingControlWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RecordingControlWidget(ActionRecorder* recorder, QWidget *parent = nullptr);

public slots:
    void updateRecordingStatus();

private slots:
    void onStartRecording();
    void onStopRecording();
    void onPauseRecording();
    void onAddComment();
    void onSaveScript();
    void onPreviewScript();
    void onClearRecording();
    void updateTimer();
    void onRecordingStarted(const QString& testName);
    void onRecordingStopped(int actionCount, qint64 durationMs);
    void onActionRecorded(const QString& description, int actionNumber);

private:
    ActionRecorder* m_recorder;
    
    // Control buttons
    QPushButton* m_startBtn;
    QPushButton* m_stopBtn;
    QPushButton* m_pauseBtn;
    QPushButton* m_commentBtn;
    QPushButton* m_saveBtn;
    QPushButton* m_previewBtn;
    QPushButton* m_clearBtn;
    
    // Status display
    QLabel* m_statusLabel;
    QLabel* m_timerLabel;
    QLabel* m_actionCountLabel;
    
    // Input controls
    QLineEdit* m_testNameEdit;
    QTextEdit* m_commentEdit;
    QComboBox* m_formatCombo;
    
    // Action list
    QListWidget* m_actionList;
    
    // Update timer
    QTimer* m_updateTimer;
    
    void setupUI();
    void connectSignals();
    void updateButtonStates();
    void addActionToList(const QString& description, int actionNumber);
    void showCommentDialog();
    void saveScriptToFile();
    void showScriptPreview();
};

#endif // ACTIONRECORDER_H
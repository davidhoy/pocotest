#include "actionrecorder.h"
#include "devicemainwindow.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QRegularExpression>
#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QListWidget>
#include <QGroupBox>
#include <QInputDialog>
#include <QDialogButtonBox>
#include <QDialog>
#include <QTimer>

ActionRecorder::ActionRecorder(DeviceMainWindow* mainWindow, QObject *parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
    , m_recording(false)
    , m_paused(false)
    , m_recordingStartTime(0)
    , m_lastActionTime(0)
    , m_pausedTime(0)
    , m_totalPausedDuration(0)
    , m_minimumWaitMs(100)
    , m_maximumWaitMs(30000)
    , m_autoDetectWaits(true)
    , m_includeTimestamps(true)
{
}

ActionRecorder::~ActionRecorder()
{
}

void ActionRecorder::startRecording(const QString& testName)
{
    if (m_recording) {
        stopRecording();
    }
    
    m_testName = sanitizeTestName(testName);
    m_actions.clear();
    m_recording = true;
    m_paused = false;
    m_recordingStartTime = QDateTime::currentMSecsSinceEpoch();
    m_lastActionTime = m_recordingStartTime;
    m_totalPausedDuration = 0;
    
    // Add initial comment
    QVariantMap params;
    params["message"] = QString("Recording started: %1").arg(m_testName);
    addAction(RecordedAction::UserComment, params, "Recording started", false);
    
    emit recordingStarted(m_testName);
}

void ActionRecorder::stopRecording()
{
    if (!m_recording) return;
    
    m_recording = false;
    m_paused = false;
    
    qint64 duration = recordingDuration();
    emit recordingStopped(m_actions.size(), duration);
}

void ActionRecorder::pauseRecording()
{
    if (!m_recording || m_paused) return;
    
    m_paused = true;
    m_pausedTime = QDateTime::currentMSecsSinceEpoch();
    emit recordingPaused();
}

void ActionRecorder::resumeRecording()
{
    if (!m_recording || !m_paused) return;
    
    qint64 pauseDuration = QDateTime::currentMSecsSinceEpoch() - m_pausedTime;
    m_totalPausedDuration += pauseDuration;
    m_paused = false;
    emit recordingResumed();
}

qint64 ActionRecorder::recordingDuration() const
{
    if (!m_recording) return 0;
    
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    qint64 totalDuration = currentTime - m_recordingStartTime - m_totalPausedDuration;
    
    if (m_paused) {
        totalDuration -= (currentTime - m_pausedTime);
    }
    
    return totalDuration;
}

void ActionRecorder::recordPGNSent(int pgn, const QString& data, int destination)
{
    if (!isRecording()) return;
    
    insertAutoWait();
    
    QVariantMap params;
    params["pgn"] = pgn;
    params["data"] = data;
    params["destination"] = destination;
    
    QString desc = QString("Send PGN %1 to destination %2")
                  .arg(pgn)
                  .arg(destination == 255 ? "broadcast" : QString("0x%1").arg(destination, 2, 16, QChar('0')).toUpper());
    
    addAction(RecordedAction::SendPGN, params, desc);
}

void ActionRecorder::recordPGNSentToDevice(int pgn, const QString& data, const QString& deviceAddress)
{
    if (!isRecording()) return;
    
    insertAutoWait();
    
    QVariantMap params;
    params["pgn"] = pgn;
    params["data"] = data;
    params["deviceAddress"] = deviceAddress;
    
    QString desc = QString("Send PGN %1 to device %2").arg(pgn).arg(deviceAddress);
    addAction(RecordedAction::SendPGNToDevice, params, desc);
}

void ActionRecorder::recordUserComment(const QString& comment)
{
    if (!isRecording()) return;
    
    QVariantMap params;
    params["message"] = comment;
    
    addAction(RecordedAction::UserComment, params, QString("Comment: %1").arg(comment));
}

void ActionRecorder::insertAutoWait()
{
    if (!m_autoDetectWaits || m_actions.isEmpty()) return;
    
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    int waitTime = calculateWaitTime(m_lastActionTime, currentTime);
    
    if (waitTime >= m_minimumWaitMs) {
        QVariantMap params;
        params["duration"] = waitTime;
        
        QString desc = QString("Wait %1").arg(formatDuration(waitTime));
        addAction(RecordedAction::AutoWait, params, desc, false);
    }
}

void ActionRecorder::addAction(RecordedAction::Type type, const QVariantMap& params, 
                              const QString& description, bool userInitiated)
{
    if (!m_recording || m_paused) return;
    
    RecordedAction action(type, params, description, userInitiated);
    m_actions.append(action);
    m_lastActionTime = action.timestamp;
    
    emit actionRecorded(description, m_actions.size());
}

QString ActionRecorder::generateJavaScript() const
{
    QString script;
    QString functionName = sanitizeTestName(m_testName);
    
    // Function header
    script += QString("// Generated test script: \"%1\"\n").arg(m_testName);
    script += QString("// Recorded on: %1\n").arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    script += QString("// Duration: %1\n").arg(formatDuration(recordingDuration()));
    script += QString("// Actions: %1\n\n").arg(m_actions.size());
    
    script += QString("function %1() {\n").arg(functionName);
    script += QString("    startTest(\"%1\");\n").arg(escapeString(m_testName));
    script += QString("    log(\"Recorded test - %1\");\n\n").arg(escapeString(m_testName));
    
    script += "    try {\n";
    
    // Generate actions
    for (int i = 0; i < m_actions.size(); ++i) {
        const RecordedAction& action = m_actions[i];
        QString actionCode = generateJavaScriptAction(action, i);
        if (!actionCode.isEmpty()) {
            script += actionCode;
        }
    }
    
    // Function footer
    script += "\n        log(\"SUCCESS: Test completed successfully\");\n";
    script += "        endTest(true);\n\n";
    script += "    } catch (error) {\n";
    script += "        log(\"FAIL: \" + error);\n";
    script += "        endTest(false);\n";
    script += "    }\n";
    script += "}\n";
    
    return script;
}

QString ActionRecorder::generateJavaScriptAction(const RecordedAction& action, int index) const
{
    Q_UNUSED(index)
    QString code;
    QString indent = "        ";
    
    switch (action.type) {
    case RecordedAction::SendPGN: {
        int pgn = action.parameters["pgn"].toInt();
        QString data = action.parameters["data"].toString();
        int dest = action.parameters["destination"].toInt();
        
        code += QString("%1// %2 (recorded at %3)\n")
               .arg(indent)
               .arg(action.description)
               .arg(formatTimestamp(action.timestamp));
        code += QString("%1sendPGN(%2, \"%3\", 0x%4);\n")
               .arg(indent)
               .arg(pgn)
               .arg(data)
               .arg(dest, 2, 16, QChar('0')).toUpper();
        code += QString("%1log(\"%2\");\n\n")
               .arg(indent)
               .arg(escapeString(action.description));
        break;
    }
    
    case RecordedAction::SendPGNToDevice: {
        int pgn = action.parameters["pgn"].toInt();
        QString data = action.parameters["data"].toString();
        QString device = action.parameters["deviceAddress"].toString();
        
        code += QString("%1// %2 (recorded at %3)\n")
               .arg(indent)
               .arg(action.description)
               .arg(formatTimestamp(action.timestamp));
        code += QString("%1sendPGNToDevice(%2, \"%3\", \"%4\");\n")
               .arg(indent)
               .arg(pgn)
               .arg(data)
               .arg(device);
        code += QString("%1log(\"%2\");\n\n")
               .arg(indent)
               .arg(escapeString(action.description));
        break;
    }
    
    case RecordedAction::AutoWait:
    case RecordedAction::ManualWait: {
        int duration = action.parameters["duration"].toInt();
        code += QString("%1// %2 (recorded delay: %3)\n")
               .arg(indent)
               .arg(action.description)
               .arg(formatDuration(duration));
        code += QString("%1waitMs(%2);\n\n")
               .arg(indent)
               .arg(duration);
        break;
    }
    
    case RecordedAction::UserComment: {
        QString message = action.parameters["message"].toString();
        if (!message.startsWith("Recording")) { // Skip recording start/stop comments
            code += QString("%1// User comment: %2\n")
                   .arg(indent)
                   .arg(escapeString(message));
            code += QString("%1log(\"%2\");\n\n")
                   .arg(indent)
                   .arg(escapeString(message));
        }
        break;
    }
    
    case RecordedAction::ClearLog:
        code += QString("%1// %2 (recorded at %3)\n")
               .arg(indent)
               .arg(action.description)
               .arg(formatTimestamp(action.timestamp));
        code += QString("%1clearLog();\n\n").arg(indent);
        break;
        
    default:
        // Other action types can be added here
        break;
    }
    
    return code;
}

QString ActionRecorder::sanitizeTestName(const QString& name) const
{
    QString sanitized = name;
    
    // Remove invalid characters for function names
    sanitized.replace(QRegularExpression("[^a-zA-Z0-9_]"), "_");
    
    // Ensure it starts with a letter
    if (!sanitized.isEmpty() && sanitized[0].isDigit()) {
        sanitized.prepend("test_");
    }
    
    // Ensure it's not empty
    if (sanitized.isEmpty()) {
        sanitized = "recordedTest";
    }
    
    return sanitized;
}

QString ActionRecorder::escapeString(const QString& str) const
{
    QString escaped = str;
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");
    escaped.replace("\n", "\\n");
    escaped.replace("\r", "\\r");
    escaped.replace("\t", "\\t");
    return escaped;
}

QString ActionRecorder::formatTimestamp(qint64 timestamp) const
{
    qint64 relativeTime = timestamp - m_recordingStartTime;
    return formatDuration(relativeTime);
}

QString ActionRecorder::formatDuration(qint64 durationMs) const
{
    if (durationMs < 1000) {
        return QString("%1ms").arg(durationMs);
    } else if (durationMs < 60000) {
        return QString("%1.%2s").arg(durationMs / 1000).arg((durationMs % 1000) / 100);
    } else {
        int minutes = durationMs / 60000;
        int seconds = (durationMs % 60000) / 1000;
        return QString("%1m %2s").arg(minutes).arg(seconds);
    }
}

int ActionRecorder::calculateWaitTime(qint64 fromTime, qint64 toTime) const
{
    int waitTime = static_cast<int>(toTime - fromTime);
    return qBound(0, waitTime, m_maximumWaitMs);
}

QString ActionRecorder::generateJsonTest() const
{
    QJsonObject jsonTest = generateJsonObject();
    QJsonDocument doc(jsonTest);
    return doc.toJson(QJsonDocument::Indented);
}

QJsonObject ActionRecorder::generateJsonObject() const
{
    QJsonObject testObject;
    
    testObject["name"] = m_testName.isEmpty() ? "Generated Test" : m_testName;
    testObject["description"] = QString("Auto-generated test with %1 actions").arg(m_actions.size());
    testObject["timeout"] = 30; // 30 seconds default timeout
    testObject["created"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QJsonArray actionsArray;
    for (const RecordedAction& action : m_actions) {
        QJsonObject stepObject = generateJsonStep(action);
        if (!stepObject.isEmpty()) {
            actionsArray.append(stepObject);
        }
    }
    
    testObject["actions"] = actionsArray;
    return testObject;
}

QJsonObject ActionRecorder::generateJsonStep(const RecordedAction& action) const
{
    QJsonObject step;
    
    switch (action.type) {
    case RecordedAction::SendPGN: {
        step["type"] = "sendPGN";
        step["pgn"] = action.parameters["pgn"].toInt();
        step["data"] = action.parameters["data"].toString();
        step["destination"] = action.parameters["destination"].toInt();
        step["description"] = action.description;
        break;
    }
    case RecordedAction::SendPGNToDevice: {
        step["type"] = "sendPGNToDevice";
        step["pgn"] = action.parameters["pgn"].toInt();
        step["data"] = action.parameters["data"].toString();
        step["deviceAddress"] = action.parameters["deviceAddress"].toString();
        step["description"] = action.description;
        break;
    }
    case RecordedAction::SetFilter: {
        step["type"] = "setFilter";
        step["sourceFilter"] = action.parameters["sourceFilter"].toString();
        step["destinationFilter"] = action.parameters["destinationFilter"].toString();
        step["description"] = action.description;
        break;
    }
    case RecordedAction::ClearLog: {
        step["type"] = "clearLog";
        step["description"] = action.description;
        break;
    }
    case RecordedAction::DeviceEnumeration: {
        step["type"] = "enumerateDevices";
        step["description"] = action.description;
        break;
    }
    case RecordedAction::SelectDevice: {
        step["type"] = "selectDevice";
        step["deviceAddress"] = action.parameters["deviceAddress"].toString();
        step["description"] = action.description;
        break;
    }
    case RecordedAction::UserComment: {
        step["type"] = "comment";
        step["message"] = action.parameters["comment"].toString();
        step["description"] = action.description;
        break;
    }
    case RecordedAction::ManualWait:
    case RecordedAction::AutoWait: {
        step["type"] = "wait";
        step["duration"] = action.parameters["duration"].toInt();
        step["description"] = action.description;
        break;
    }
    case RecordedAction::Assertion: {
        step["type"] = "assert";
        step["condition"] = action.parameters["condition"].toString();
        step["description"] = action.parameters["description"].toString();
        break;
    }
    case RecordedAction::MessageReceived: {
        step["type"] = "waitForMessage";
        step["pgn"] = action.parameters["pgn"].toInt();
        step["source"] = action.parameters["source"].toString();
        step["description"] = action.description;
        break;
    }
    case RecordedAction::DeviceDiscovered: {
        step["type"] = "waitForDevice";
        step["deviceAddress"] = action.parameters["deviceAddress"].toString();
        step["manufacturer"] = action.parameters["manufacturer"].toString();
        step["description"] = action.description;
        break;
    }
    }
    
    if (m_includeTimestamps && !step.isEmpty()) {
        step["timestamp"] = formatTimestamp(action.timestamp);
    }
    
    return step;
}

void ActionRecorder::recordLogClear()
{
    if (!m_recording || m_paused) return;
    
    QVariantMap params;
    addAction(RecordedAction::ClearLog, params, "Clear log");
}

void ActionRecorder::recordFilterChange(const QString& sourceFilter, const QString& destFilter)
{
    if (!m_recording || m_paused) return;
    
    QVariantMap params;
    params["sourceFilter"] = sourceFilter;
    params["destinationFilter"] = destFilter;
    
    QString desc = QString("Set filters - Source: %1, Destination: %2")
                  .arg(sourceFilter.isEmpty() ? "None" : sourceFilter)
                  .arg(destFilter.isEmpty() ? "None" : destFilter);
    
    addAction(RecordedAction::SetFilter, params, desc);
}

void ActionRecorder::recordDeviceDiscovered(const QString& deviceAddress, const QString& manufacturer)
{
    if (!m_recording || m_paused) return;
    
    QVariantMap params;
    params["deviceAddress"] = deviceAddress;
    params["manufacturer"] = manufacturer;
    
    QString desc = QString("Device discovered: %1 (%2)")
                  .arg(deviceAddress)
                  .arg(manufacturer);
    
    addAction(RecordedAction::DeviceDiscovered, params, desc, false); // Not user-initiated
}

void ActionRecorder::recordMessageReceived(int pgn, const QString& source)
{
    if (!m_recording || m_paused) return;
    
    QVariantMap params;
    params["pgn"] = pgn;
    params["source"] = source;
    
    QString desc = QString("Message received: PGN %1 from %2")
                  .arg(pgn)
                  .arg(source);
    
    addAction(RecordedAction::MessageReceived, params, desc, false); // Not user-initiated
}

void ActionRecorder::recordAssertion(const QString& condition, const QString& description)
{
    if (!m_recording || m_paused) return;
    
    QVariantMap params;
    params["condition"] = condition;
    params["description"] = description;
    
    QString desc = QString("Assert: %1 (%2)")
                  .arg(description)
                  .arg(condition);
    
    addAction(RecordedAction::Assertion, params, desc);
}

void ActionRecorder::recordManualWait(int durationMs)
{
    if (!m_recording || m_paused) return;
    
    QVariantMap params;
    params["duration"] = durationMs;
    
    QString desc = QString("Wait %1ms").arg(durationMs);
    
    addAction(RecordedAction::ManualWait, params, desc);
}

void ActionRecorder::recordDeviceSelection(const QString& deviceAddress)
{
    if (!m_recording || m_paused) return;
    
    QVariantMap params;
    params["deviceAddress"] = deviceAddress;
    
    QString desc = QString("Select device: %1").arg(deviceAddress);
    
    addAction(RecordedAction::SelectDevice, params, desc);
}

void ActionRecorder::recordDeviceEnumeration()
{
    if (!m_recording || m_paused) return;
    
    QVariantMap params;
    addAction(RecordedAction::DeviceEnumeration, params, "Enumerate devices");
}

// RecordingControlWidget implementation
RecordingControlWidget::RecordingControlWidget(ActionRecorder* recorder, QWidget *parent)
    : QWidget(parent)
    , m_recorder(recorder)
    , m_updateTimer(new QTimer(this))
{
    setupUI();
    connectSignals();
    
    m_updateTimer->setInterval(100); // Update every 100ms
    connect(m_updateTimer, &QTimer::timeout, this, &RecordingControlWidget::updateTimer);
    
    updateButtonStates();
}

void RecordingControlWidget::setupUI()
{
    setWindowTitle("Recording Controls");
    
    auto* mainLayout = new QVBoxLayout(this);
    
    // Test name input
    auto* nameGroup = new QGroupBox("Test Configuration", this);
    auto* nameLayout = new QHBoxLayout(nameGroup);
    nameLayout->addWidget(new QLabel("Test Name:"));
    m_testNameEdit = new QLineEdit(this);
    m_testNameEdit->setPlaceholderText("Enter test name...");
    nameLayout->addWidget(m_testNameEdit);
    mainLayout->addWidget(nameGroup);
    
    // Control buttons
    auto* controlGroup = new QGroupBox("Recording Controls", this);
    auto* controlLayout = new QHBoxLayout(controlGroup);
    
    m_startBtn = new QPushButton("🔴 Start", this);
    m_stopBtn = new QPushButton("⏹️ Stop", this);
    m_pauseBtn = new QPushButton("⏸️ Pause", this);
    m_commentBtn = new QPushButton("💬 Comment", this);
    
    controlLayout->addWidget(m_startBtn);
    controlLayout->addWidget(m_stopBtn);
    controlLayout->addWidget(m_pauseBtn);
    controlLayout->addWidget(m_commentBtn);
    mainLayout->addWidget(controlGroup);
    
    // Status display
    auto* statusGroup = new QGroupBox("Recording Status", this);
    auto* statusLayout = new QVBoxLayout(statusGroup);
    
    m_statusLabel = new QLabel("Ready to record", this);
    m_timerLabel = new QLabel("00:00.0", this);
    m_actionCountLabel = new QLabel("Actions: 0", this);
    
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addWidget(m_timerLabel);
    statusLayout->addWidget(m_actionCountLabel);
    mainLayout->addWidget(statusGroup);
    
    // Action list
    auto* actionsGroup = new QGroupBox("Recorded Actions", this);
    auto* actionsLayout = new QVBoxLayout(actionsGroup);
    
    m_actionList = new QListWidget(this);
    m_actionList->setMaximumHeight(150);
    actionsLayout->addWidget(m_actionList);
    mainLayout->addWidget(actionsGroup);
    
    // Script generation
    auto* scriptGroup = new QGroupBox("Script Generation", this);
    auto* scriptLayout = new QHBoxLayout(scriptGroup);
    
    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItems({"JavaScript", "JSON"});
    m_previewBtn = new QPushButton("Preview", this);
    m_saveBtn = new QPushButton("Save Script", this);
    m_clearBtn = new QPushButton("Clear", this);
    
    scriptLayout->addWidget(new QLabel("Format:"));
    scriptLayout->addWidget(m_formatCombo);
    scriptLayout->addStretch();
    scriptLayout->addWidget(m_previewBtn);
    scriptLayout->addWidget(m_saveBtn);
    scriptLayout->addWidget(m_clearBtn);
    mainLayout->addWidget(scriptGroup);
}

void RecordingControlWidget::connectSignals()
{
    connect(m_startBtn, &QPushButton::clicked, this, &RecordingControlWidget::onStartRecording);
    connect(m_stopBtn, &QPushButton::clicked, this, &RecordingControlWidget::onStopRecording);
    connect(m_pauseBtn, &QPushButton::clicked, this, &RecordingControlWidget::onPauseRecording);
    connect(m_commentBtn, &QPushButton::clicked, this, &RecordingControlWidget::onAddComment);
    connect(m_previewBtn, &QPushButton::clicked, this, &RecordingControlWidget::onPreviewScript);
    connect(m_saveBtn, &QPushButton::clicked, this, &RecordingControlWidget::onSaveScript);
    connect(m_clearBtn, &QPushButton::clicked, this, &RecordingControlWidget::onClearRecording);
    
    // Recorder signals
    connect(m_recorder, &ActionRecorder::recordingStarted, this, &RecordingControlWidget::onRecordingStarted);
    connect(m_recorder, &ActionRecorder::recordingStopped, this, &RecordingControlWidget::onRecordingStopped);
    connect(m_recorder, &ActionRecorder::actionRecorded, this, &RecordingControlWidget::onActionRecorded);
}

void RecordingControlWidget::onStartRecording()
{
    QString testName = m_testNameEdit->text().trimmed();
    if (testName.isEmpty()) {
        testName = QString("Test_%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    }
    
    m_recorder->startRecording(testName);
}

void RecordingControlWidget::onStopRecording()
{
    m_recorder->stopRecording();
}

void RecordingControlWidget::onPauseRecording()
{
    if (m_recorder->isPaused()) {
        m_recorder->resumeRecording();
        m_pauseBtn->setText("⏸️ Pause");
    } else {
        m_recorder->pauseRecording();
        m_pauseBtn->setText("▶️ Resume");
    }
}

void RecordingControlWidget::onAddComment()
{
    bool ok;
    QString comment = QInputDialog::getText(this, "Add Comment", 
                                          "Enter comment:", QLineEdit::Normal, "", &ok);
    if (ok && !comment.isEmpty()) {
        m_recorder->recordUserComment(comment);
    }
}

void RecordingControlWidget::onRecordingStarted(const QString& testName)
{
    m_statusLabel->setText(QString("Recording: %1").arg(testName));
    m_updateTimer->start();
    updateButtonStates();
    m_actionList->clear();
}

void RecordingControlWidget::onRecordingStopped(int actionCount, qint64 durationMs)
{
    m_statusLabel->setText(QString("Stopped - %1 actions, %2")
                          .arg(actionCount)
                          .arg(m_recorder->formatDuration(durationMs)));
    m_updateTimer->stop();
    updateButtonStates();
}

void RecordingControlWidget::onActionRecorded(const QString& description, int actionNumber)
{
    addActionToList(description, actionNumber);
    m_actionCountLabel->setText(QString("Actions: %1").arg(actionNumber));
}

void RecordingControlWidget::updateButtonStates()
{
    bool recording = m_recorder->isRecording();
    bool paused = m_recorder->isPaused();
    
    m_startBtn->setEnabled(!recording);
    m_stopBtn->setEnabled(recording);
    m_pauseBtn->setEnabled(recording);
    m_commentBtn->setEnabled(recording && !paused);
    m_testNameEdit->setEnabled(!recording);
    
    m_previewBtn->setEnabled(m_recorder->actionCount() > 0);
    m_saveBtn->setEnabled(m_recorder->actionCount() > 0);
    m_clearBtn->setEnabled(!recording && m_recorder->actionCount() > 0);
}

void RecordingControlWidget::updateTimer()
{
    if (m_recorder->isRecording()) {
        qint64 duration = m_recorder->recordingDuration();
        int totalSeconds = duration / 1000;
        int minutes = totalSeconds / 60;
        int seconds = totalSeconds % 60;
        int deciseconds = (duration % 1000) / 100;
        
        m_timerLabel->setText(QString("%1:%2.%3")
                             .arg(minutes, 2, 10, QChar('0'))
                             .arg(seconds, 2, 10, QChar('0'))
                             .arg(deciseconds));
    }
}

void RecordingControlWidget::addActionToList(const QString& description, int actionNumber)
{
    QString itemText = QString("%1. %2").arg(actionNumber, 3, 10, QChar('0')).arg(description);
    m_actionList->addItem(itemText);
    m_actionList->scrollToBottom();
}

void RecordingControlWidget::updateRecordingStatus()
{
    updateButtonStates();
    if (m_recorder->isRecording()) {
        qint64 duration = m_recorder->recordingDuration();
        m_timerLabel->setText(QString("Duration: %1").arg(m_recorder->formatDuration(duration)));
        m_actionCountLabel->setText(QString("Actions: %1").arg(m_recorder->actionCount()));
    }
}

void RecordingControlWidget::onPreviewScript()
{
    QString script;
    if (m_formatCombo->currentText() == "JavaScript") {
        script = m_recorder->generateJavaScript();
    } else {
        script = m_recorder->generateJsonTest();
    }
    
    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle("Script Preview");
    dialog->resize(800, 600);
    
    auto* layout = new QVBoxLayout(dialog);
    auto* textEdit = new QTextEdit(dialog);
    textEdit->setPlainText(script);
    textEdit->setReadOnly(true);
    layout->addWidget(textEdit);
    
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    connect(buttonBox, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    layout->addWidget(buttonBox);
    
    dialog->exec();
    dialog->deleteLater();
}

void RecordingControlWidget::onSaveScript()
{
    QString filter;
    QString script;
    QString defaultSuffix;
    
    if (m_formatCombo->currentText() == "JavaScript") {
        script = m_recorder->generateJavaScript();
        filter = "JavaScript Files (*.js);;All Files (*)";
        defaultSuffix = ".js";
    } else {
        script = m_recorder->generateJsonTest();
        filter = "JSON Files (*.json);;All Files (*)";
        defaultSuffix = ".json";
    }
    
    QString testName = m_recorder->getTestName();
    QString suggestedName = testName.isEmpty() ? "test_script" : testName;
    
    QString fileName = QFileDialog::getSaveFileName(this, "Save Script", 
                                                   suggestedName + defaultSuffix, filter);
    
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << script;
            file.close();
            
            QMessageBox::information(this, "Script Saved", 
                                   QString("Script saved to: %1").arg(fileName));
        } else {
            QMessageBox::warning(this, "Save Error", 
                                QString("Could not save script to: %1").arg(fileName));
        }
    }
}

void RecordingControlWidget::onClearRecording()
{
    if (m_recorder->actionCount() > 0) {
        int ret = QMessageBox::question(this, "Clear Recording", 
                                      "Are you sure you want to clear all recorded actions?",
                                      QMessageBox::Yes | QMessageBox::No,
                                      QMessageBox::No);
        
        if (ret == QMessageBox::Yes) {
            m_recorder->clearActions();
            m_actionList->clear();
            m_actionCountLabel->setText("Actions: 0");
            updateButtonStates();
        }
    }
}
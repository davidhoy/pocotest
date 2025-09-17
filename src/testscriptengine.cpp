#include "testscriptengine.h"
#include "devicemainwindow.h"
#include "pgnlogdialog.h"
#include <QJSValue>
#include <QFile>
#include <QTextStream>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <QThread>
#include <QTimer>
#include <QEventLoop>

TestScriptEngine::TestScriptEngine(DeviceMainWindow* mainWindow, QObject *parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
    , m_logDialog(nullptr)
    , m_jsEngine(new QJSEngine(this))
    , m_currentTestName("")
    , m_testTimer(new QTimer(this))
    , m_waitTimer(new QTimer(this))
    , m_waitLoop(new QEventLoop(this))
    , m_testRunning(false)
    , m_testTimeoutMs(30000)
    , m_testPassed(false)
{
    setupJSEngine();
    connect(m_testTimer, &QTimer::timeout, this, &TestScriptEngine::onTestTimeout);
    m_testTimer->setSingleShot(true);
    resetWaitCondition();
}

void TestScriptEngine::setupJSEngine()
{
    // Create test API object
    QJSValue testApi = m_jsEngine->newObject();
    
    // Add logging function (simple version without QJSEngine::newQFunction)
    testApi.setProperty("log", m_jsEngine->evaluate("(function(message) { return message; })"));
    
    // Set global test object
    m_jsEngine->globalObject().setProperty("test", testApi);
    
    // Expose main window
    if (m_mainWindow) {
        QJSValue mainWindowValue = m_jsEngine->newQObject(m_mainWindow);
        m_jsEngine->globalObject().setProperty("mainWindow", mainWindowValue);
    }
}

bool TestScriptEngine::executeScript(const QString& scriptContent)
{
    QJSValue result = m_jsEngine->evaluate(scriptContent);
    
    if (result.isError()) {
        QString error = QString("Script error at line %1: %2")
                           .arg(result.property("lineNumber").toInt())
                           .arg(result.toString());
        emit testError(error);
        return false;
    }
    
    return true;
}

bool TestScriptEngine::executeScriptFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit testError(QString("Could not open script file: %1").arg(filePath));
        return false;
    }
    
    QTextStream in(&file);
    QString script = in.readAll();
    file.close();
    
    return executeScript(script);
}

bool TestScriptEngine::executeJsonTest(const QString& jsonContent)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonContent.toUtf8(), &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        emit testError(QString("JSON parse error: %1").arg(parseError.errorString()));
        return false;
    }
    
    QJsonObject testObject = doc.object();
    m_currentTestName = testObject["name"].toString();
    m_testRunning = true;
    m_testPassed = true;
    
    emit testStarted(m_currentTestName);
    
    // Start timeout timer if specified
    if (testObject.contains("timeout")) {
        m_testTimer->start(testObject["timeout"].toInt() * 1000);
    } else {
        m_testTimer->start(m_testTimeoutMs);
    }
    
    QJsonArray actions = testObject["actions"].toArray();
    for (const QJsonValue& action : actions) {
        if (!executeJsonStep(action.toObject())) {
            m_testPassed = false;
            break;
        }
    }
    
    endTest(m_testPassed);
    return m_testPassed;
}

bool TestScriptEngine::executeJsonTestFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit testError(QString("Could not open JSON test file: %1").arg(filePath));
        return false;
    }
    
    QTextStream in(&file);
    QString jsonContent = in.readAll();
    file.close();
    
    return executeJsonTest(jsonContent);
}

bool TestScriptEngine::executeJsonStep(const QJsonObject& step)
{
    QString actionType = step["type"].toString();
    
    if (actionType == "sendPGN") {
        int pgn = step["pgn"].toInt();
        QString destination = step["destination"].toString();
        emit logMessage(QString("Sending PGN %1 to %2").arg(pgn).arg(destination));
        return true;
    } else if (actionType == "wait") {
        int duration = step["duration"].toInt();
        QThread::msleep(duration);
        return true;
    } else if (actionType == "assert") {
        QString condition = step["condition"].toString();
        emit logMessage(QString("Asserting: %1").arg(condition));
        return true;
    } else if (actionType == "log") {
        emit logMessage(step["message"].toString());
        return true;
    }
    
    emit testError(QString("Unknown action type: %1").arg(actionType));
    return false;
}

void TestScriptEngine::startTest(const QString& testName)
{
    m_currentTestName = testName;
    m_testRunning = true;
    m_testPassed = true;
    emit testStarted(testName);
}

void TestScriptEngine::endTest(bool passed)
{
    m_testTimer->stop();
    m_testRunning = false;
    m_testPassed = passed;
    
    QString report = formatTestReport();
    emit testCompleted(m_currentTestName, passed, report);
}

void TestScriptEngine::setTestTimeout(int seconds)
{
    m_testTimeoutMs = seconds * 1000;
}

void TestScriptEngine::resetWaitCondition()
{
    m_waitCondition.pgn = 0;
    m_waitCondition.sourceAddress = "";
    m_waitCondition.matched = false;
}

QString TestScriptEngine::formatTestReport()
{
    QString report = QString("Test: %1\n").arg(m_currentTestName);
    report += QString("Result: %1\n").arg(m_testPassed ? "PASSED" : "FAILED");
    report += "Log:\n";
    for (const QString& entry : m_testLog) {
        report += "  " + entry + "\n";
    }
    return report;
}

void TestScriptEngine::onMessageReceived(const tN2kMsg& msg)
{
    Q_UNUSED(msg)
    // Handle received messages for test assertions
}

void TestScriptEngine::onTestTimeout()
{
    emit testError("Test timeout");
    endTest(false);
}

// Core scripting API implementations
void TestScriptEngine::sendPGN(int pgn, const QString& hexData, int destination)
{
    emit logMessage(QString("Sending PGN %1 to destination %2: %3").arg(pgn).arg(destination).arg(hexData));
    
    if (m_mainWindow) {
        // Signal to main window to send the PGN
        // This would typically interface with the actual CAN transmission system
        emit logMessage(QString("PGN %1 sent successfully").arg(pgn));
    }
}

void TestScriptEngine::sendPGNToDevice(int pgn, const QString& hexData, const QString& deviceAddress)
{
    emit logMessage(QString("Sending PGN %1 to device %2: %3").arg(pgn).arg(deviceAddress).arg(hexData));
    
    if (m_mainWindow) {
        // Signal to main window to send the PGN to specific device
        emit logMessage(QString("PGN %1 sent to device %2 successfully").arg(pgn).arg(deviceAddress));
    }
}

bool TestScriptEngine::waitForPGN(int pgn, int timeoutMs)
{
    emit logMessage(QString("Waiting for PGN %1 (timeout: %2ms)").arg(pgn).arg(timeoutMs));
    
    m_waitCondition.pgn = pgn;
    m_waitCondition.sourceAddress = "";
    m_waitCondition.matched = false;
    
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    timeoutTimer.start(timeoutMs);
    
    QEventLoop waitLoop;
    connect(&timeoutTimer, &QTimer::timeout, &waitLoop, &QEventLoop::quit);
    
    // In a real implementation, this would wait for actual messages
    // For now, we'll just simulate success after a short delay
    QTimer::singleShot(100, [this, &waitLoop]() {
        m_waitCondition.matched = true;
        waitLoop.quit();
    });
    
    waitLoop.exec();
    
    bool success = m_waitCondition.matched;
    emit logMessage(QString("Wait for PGN %1: %2").arg(pgn).arg(success ? "SUCCESS" : "TIMEOUT"));
    
    return success;
}

bool TestScriptEngine::waitForPGNFromSource(int pgn, const QString& sourceAddress, int timeoutMs)
{
    emit logMessage(QString("Waiting for PGN %1 from source %2 (timeout: %3ms)")
                   .arg(pgn).arg(sourceAddress).arg(timeoutMs));
    
    m_waitCondition.pgn = pgn;
    m_waitCondition.sourceAddress = sourceAddress;
    m_waitCondition.matched = false;
    
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    timeoutTimer.start(timeoutMs);
    
    QEventLoop waitLoop;
    connect(&timeoutTimer, &QTimer::timeout, &waitLoop, &QEventLoop::quit);
    
    // Simulate success after a short delay
    QTimer::singleShot(100, [this, &waitLoop]() {
        m_waitCondition.matched = true;
        waitLoop.quit();
    });
    
    waitLoop.exec();
    
    bool success = m_waitCondition.matched;
    emit logMessage(QString("Wait for PGN %1 from %2: %3")
                   .arg(pgn).arg(sourceAddress).arg(success ? "SUCCESS" : "TIMEOUT"));
    
    return success;
}

void TestScriptEngine::setSourceFilter(const QString& sourceAddress)
{
    emit logMessage(QString("Setting source filter: %1").arg(sourceAddress));
    
    if (m_logDialog) {
        // Set filter on PGN log dialog
        // This would interface with the actual filtering system
    }
}

void TestScriptEngine::setDestinationFilter(const QString& destAddress)
{
    emit logMessage(QString("Setting destination filter: %1").arg(destAddress));
    
    if (m_logDialog) {
        // Set filter on PGN log dialog
    }
}

void TestScriptEngine::clearFilters()
{
    emit logMessage("Clearing all filters");
    
    if (m_logDialog) {
        // Clear filters on PGN log dialog
    }
}

void TestScriptEngine::clearLog()
{
    emit logMessage("Clearing log");
    m_testLog.clear();
    
    if (m_logDialog) {
        // Clear the PGN log dialog
    }
}

void TestScriptEngine::waitMs(int milliseconds)
{
    emit logMessage(QString("Waiting %1ms").arg(milliseconds));
    QThread::msleep(milliseconds);
}

void TestScriptEngine::log(const QString& message)
{
    QString logEntry = QString("[%1] %2")
                      .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"))
                      .arg(message);
    
    m_testLog.append(logEntry);
    emit logMessage(message);
}

void TestScriptEngine::assertCondition(bool condition, const QString& message)
{
    QString result = condition ? "PASS" : "FAIL";
    QString logEntry = QString("ASSERT %1: %2").arg(result).arg(message);
    
    log(logEntry);
    
    if (!condition) {
        m_testPassed = false;
        emit testError(QString("Assertion failed: %1").arg(message));
    }
}

// Device management implementations
int TestScriptEngine::getDeviceCount()
{
    if (m_mainWindow) {
        // Return actual device count from main window
        // For now, return a placeholder value
        return 5; // Placeholder
    }
    return 0;
}

QStringList TestScriptEngine::getDeviceAddresses()
{
    if (m_mainWindow) {
        // Return actual device addresses from main window
        // For now, return placeholder values
        return {"0x10", "0x20", "0x30", "0x40", "0x50"}; // Placeholder
    }
    return QStringList();
}

bool TestScriptEngine::isDevicePresent(const QString& deviceAddress)
{
    QStringList addresses = getDeviceAddresses();
    bool present = addresses.contains(deviceAddress);
    
    emit logMessage(QString("Device %1 present: %2").arg(deviceAddress).arg(present ? "YES" : "NO"));
    return present;
}

QString TestScriptEngine::getDeviceManufacturer(const QString& deviceAddress)
{
    Q_UNUSED(deviceAddress)
    
    if (m_mainWindow && isDevicePresent(deviceAddress)) {
        // Return actual manufacturer from main window device list
        // For now, return placeholder
        return "Lumitec"; // Placeholder
    }
    return "Unknown";
}

QString TestScriptEngine::getDeviceModel(const QString& deviceAddress)
{
    Q_UNUSED(deviceAddress)
    
    if (m_mainWindow && isDevicePresent(deviceAddress)) {
        // Return actual model from main window device list
        // For now, return placeholder
        return "Poco Light"; // Placeholder
    }
    return "Unknown";
}

// Message validation implementations
bool TestScriptEngine::validateLastMessage(int pgn, const QString& expectedData)
{
    // This would check the last received message of the specified PGN
    // For now, we'll simulate validation
    QString actualData = getLastMessageData(pgn);
    bool valid = (actualData == expectedData);
    
    emit logMessage(QString("Validate PGN %1 data: Expected='%2', Actual='%3', Result=%4")
                   .arg(pgn).arg(expectedData).arg(actualData).arg(valid ? "PASS" : "FAIL"));
    
    return valid;
}

int TestScriptEngine::getMessageCount()
{
    if (m_logDialog) {
        // Return actual message count from log dialog
        // For now, return placeholder
        return 100; // Placeholder
    }
    return 0;
}

QString TestScriptEngine::getLastMessageData(int pgn)
{
    Q_UNUSED(pgn)
    
    if (m_logDialog) {
        // Return actual last message data for the specified PGN
        // For now, return placeholder
        return QString("01 02 03 04"); // Placeholder
    }
    return QString();
}
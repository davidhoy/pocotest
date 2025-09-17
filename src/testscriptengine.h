#ifndef TESTSCRIPTENGINE_H
#define TESTSCRIPTENGINE_H

#include <QObject>
#include <QJSEngine>
#include <QTimer>
#include <QEventLoop>
#include <QJsonObject>
#include <QJsonArray>
#include <N2kMsg.h>

class DeviceMainWindow;
class PGNLogDialog;

class TestScriptEngine : public QObject
{
    Q_OBJECT

public:
    explicit TestScriptEngine(DeviceMainWindow* mainWindow, QObject *parent = nullptr);
    
    // Execute JavaScript test script
    bool executeScript(const QString& scriptContent);
    bool executeScriptFile(const QString& filePath);
    
    // Execute JSON test definition
    bool executeJsonTest(const QString& jsonContent);
    bool executeJsonTestFile(const QString& filePath);

public slots:
    // Core scripting API - exposed to JavaScript
    void sendPGN(int pgn, const QString& hexData, int destination = 255);
    void sendPGNToDevice(int pgn, const QString& hexData, const QString& deviceAddress);
    
    bool waitForPGN(int pgn, int timeoutMs = 5000);
    bool waitForPGNFromSource(int pgn, const QString& sourceAddress, int timeoutMs = 5000);
    
    void setSourceFilter(const QString& sourceAddress);
    void setDestinationFilter(const QString& destAddress);
    void clearFilters();
    void clearLog();
    
    void waitMs(int milliseconds);
    void log(const QString& message);
    void assertCondition(bool condition, const QString& message);
    
    // Device management
    int getDeviceCount();
    QStringList getDeviceAddresses();
    bool isDevicePresent(const QString& deviceAddress);
    QString getDeviceManufacturer(const QString& deviceAddress);
    QString getDeviceModel(const QString& deviceAddress);
    
    // Message validation
    bool validateLastMessage(int pgn, const QString& expectedData);
    int getMessageCount();
    QString getLastMessageData(int pgn);
    
    // Test state
    void startTest(const QString& testName);
    void endTest(bool passed);
    void setTestTimeout(int seconds);

signals:
    void testStarted(const QString& testName);
    void testCompleted(const QString& testName, bool passed, const QString& report);
    void testError(const QString& error);
    void logMessage(const QString& message);

private slots:
    void onMessageReceived(const tN2kMsg& msg);
    void onTestTimeout();

private:
    DeviceMainWindow* m_mainWindow;
    PGNLogDialog* m_logDialog;
    QJSEngine* m_jsEngine;
    
    // Test execution state
    QString m_currentTestName;
    QTimer* m_testTimer;
    QTimer* m_waitTimer;
    QEventLoop* m_waitLoop;
    bool m_testRunning;
    int m_testTimeoutMs;
    
    // Message waiting state
    struct WaitCondition {
        int pgn;
        QString sourceAddress;
        bool matched;
    };
    WaitCondition m_waitCondition;
    
    // Test results
    QStringList m_testLog;
    bool m_testPassed;
    
    // Helper methods
    void setupJSEngine();
    bool executeJsonStep(const QJsonObject& step);
    void resetWaitCondition();
    QString formatTestReport();
};

#endif // TESTSCRIPTENGINE_H
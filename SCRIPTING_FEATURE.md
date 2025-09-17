# Lumitec Poco Tester - Automated Testing Scripting Feature

## Overview

This document outlines a comprehensive scripting feature proposal for the Lumitec Poco Tester application to enable automated testing capabilities. The feature would leverage existing application capabilities to provide a robust testing framework for NMEA2000 devices, particularly Lumitec lighting systems.

## Current Capabilities Analysis

The Lumitec Poco Tester already provides several powerful features that can be leveraged for automated testing:

### Existing Features
- **Non-modal PGN Dialog** - Can send arbitrary PGNs with custom parameters
- **PGN Log Dialog** - Captures and filters all NMEA2000 traffic with real-time updates
- **Device Discovery** - Automatic enumeration of devices on the bus
- **Message Filtering** - Source/destination filtering, PGN ignore lists
- **DBC Decoding** - Structured message interpretation using industry-standard DBC files
- **Lumitec Poco Controls** - Device-specific lighting control interface
- **Instance Conflict Detection** - Bus health monitoring and diagnostic capabilities

### Integration Points
- Qt6 application architecture with signal/slot communication
- NMEA2000 message handling and transmission
- Real-time UI updates and state management
- Device enumeration and identification
- Message logging and filtering capabilities

## Proposed Scripting Architecture

### Option 1: Embedded JavaScript Engine (Recommended)

Implement a scripting capability directly within the Qt application using QJSEngine for JavaScript execution.

#### Key Components

1. **TestScriptEngine Class** - Core scripting engine integration
2. **JavaScript API** - Exposed functions for test script interaction
3. **Test Execution Framework** - Script loading, execution, and result reporting
4. **UI Integration** - Script editor, execution controls, and result display

#### Architecture Benefits
- Leverages existing Qt infrastructure
- No external dependencies
- Real-time integration with application state
- Familiar JavaScript syntax for developers
- Can be extended for CI/CD integration

### Option 2: External Script Support

Command-line interface with JSON-based test definitions for external automation tools.

#### Features
- JSON test suite definitions
- Command-line execution interface
- Structured test reporting
- CI/CD pipeline integration

### Option 3: Python API Integration

External Python API controlling the Qt application via D-Bus or socket communication.

#### Benefits
- Powerful Python ecosystem integration
- Advanced data analysis capabilities
- Integration with existing Python test frameworks
- Remote testing capabilities

## Detailed Implementation Plan

### Phase 1: Core Scripting Infrastructure

#### 1.1 TestScriptEngine Class Implementation

```cpp
class TestScriptEngine : public QObject
{
    Q_OBJECT

public:
    explicit TestScriptEngine(DeviceMainWindow* mainWindow, QObject *parent = nullptr);
    
    // Script execution methods
    bool executeScript(const QString& scriptContent);
    bool executeScriptFile(const QString& filePath);
    bool executeJsonTest(const QString& jsonContent);
    bool executeJsonTestFile(const QString& filePath);

public slots:
    // Core API functions exposed to JavaScript
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
    
    // Device management API
    int getDeviceCount();
    QStringList getDeviceAddresses();
    bool isDevicePresent(const QString& deviceAddress);
    QString getDeviceManufacturer(const QString& deviceAddress);
    QString getDeviceModel(const QString& deviceAddress);
    
    // Message validation API
    bool validateLastMessage(int pgn, const QString& expectedData);
    int getMessageCount();
    QString getLastMessageData(int pgn);
    
    // Test state management
    void startTest(const QString& testName);
    void endTest(bool passed);
    void setTestTimeout(int seconds);

signals:
    void testStarted(const QString& testName);
    void testCompleted(const QString& testName, bool passed, const QString& report);
    void testError(const QString& error);
    void logMessage(const QString& message);
};
```

#### 1.2 JavaScript API Design

The JavaScript API provides a clean, intuitive interface for test script development:

```javascript
// Basic usage examples
sendPGN(130561, "00 FF 00 00 64 01 00 FF", 0x0E);  // Send lighting command
waitForPGN(126208, 3000);                           // Wait for acknowledgment
log("Test message");                                // Log test information
assertCondition(getDeviceCount() > 0, "No devices found");
```

#### 1.3 Test Script Examples

**Basic Lighting Control Test:**
```javascript
function testBasicLighting() {
    startTest("Basic Lighting Control");
    log("Testing basic zone lighting control...");
    
    try {
        clearLog();
        
        // Send zone 1 on, full brightness
        sendPGN(130561, "00 FF 00 00 64 01 00 FF", 0x0E);
        log("Sent zone 1 ON command");
        
        // Wait for acknowledgment
        if (!waitForPGNFromSource(126208, "0E", 3000)) {
            throw "No acknowledgment received from device";
        }
        log("Received acknowledgment");
        
        // Send zone 1 off
        sendPGN(130561, "00 FF 00 00 00 01 00 FF", 0x0E);
        log("Sent zone 1 OFF command");
        
        if (!waitForPGNFromSource(126208, "0E", 3000)) {
            throw "No acknowledgment for OFF command";
        }
        
        log("SUCCESS: Basic lighting control test passed");
        endTest(true);
        
    } catch (error) {
        log("FAIL: " + error);
        endTest(false);
    }
}
```

**Device Enumeration Test:**
```javascript
function testDeviceEnumeration() {
    startTest("Device Enumeration");
    
    try {
        clearLog();
        
        // Request ISO address claim from all devices
        sendPGN(59904, "FF FF FF FF FF FF FF FF", 255);
        waitMs(2000);
        
        var deviceCount = getDeviceCount();
        assertCondition(deviceCount >= 1, "No devices found on bus");
        
        // Check for Lumitec devices
        var devices = getDeviceAddresses();
        var lumitecFound = false;
        
        for (var i = 0; i < devices.length; i++) {
            var manufacturer = getDeviceManufacturer(devices[i]);
            if (manufacturer.includes("Lumitec")) {
                lumitecFound = true;
                break;
            }
        }
        
        assertCondition(lumitecFound, "No Lumitec devices found");
        endTest(true);
        
    } catch (error) {
        log("FAIL: " + error);
        endTest(false);
    }
}
```

### Phase 2: JSON Test Suite Support

#### 2.1 JSON Test Definition Format

```json
{
  "test_suite": {
    "name": "Lumitec Poco Automated Test Suite",
    "version": "1.0",
    "setup": {
      "interface": "vcan0",
      "timeout_seconds": 60,
      "filters": {
        "log_all_traffic": true,
        "ignore_pgns": [127250, 129026, 129029]
      }
    },
    "tests": [
      {
        "name": "Device Discovery",
        "description": "Verify devices are present and responding",
        "timeout_seconds": 30,
        "steps": [
          {
            "action": "clear_log",
            "description": "Clear existing message log"
          },
          {
            "action": "send_pgn",
            "pgn": 59904,
            "data": "FF FF FF FF FF FF FF FF",
            "destination": 255,
            "description": "Request ISO address claim from all devices"
          },
          {
            "action": "wait",
            "duration_ms": 3000,
            "description": "Allow time for device responses"
          },
          {
            "action": "validate",
            "condition": "device_count >= 1",
            "description": "At least one device must be present"
          }
        ]
      }
    ]
  }
}
```

#### 2.2 Supported Actions

| Action | Description | Parameters |
|--------|-------------|------------|
| `clear_log` | Clear message log | None |
| `send_pgn` | Send PGN message | `pgn`, `data`, `destination` |
| `wait` | Wait for specified time | `duration_ms` |
| `wait_for_pgn` | Wait for specific PGN | `pgn`, `source`, `timeout_ms` |
| `validate` | Assert condition | `condition`, `description` |
| `set_filter` | Set message filter | `source`, `destination` |
| `log` | Add log message | `message` |
| `loop` | Repeat actions | `count`, `steps` |

### Phase 3: Advanced Features

#### 3.1 Record/Playback System

The record/playback feature captures user interactions and converts them into editable scripts, dramatically simplifying test creation.

##### 3.1.1 Recording Architecture

```cpp
class ActionRecorder : public QObject
{
    Q_OBJECT

public:
    explicit ActionRecorder(DeviceMainWindow* mainWindow, QObject *parent = nullptr);
    
    void startRecording(const QString& testName);
    void stopRecording();
    void pauseRecording();
    void resumeRecording();
    
    bool isRecording() const { return m_recording; }
    QString generateScript() const;
    QString generateJsonTest() const;
    
public slots:
    // Event capture methods
    void recordPGNSent(int pgn, const QString& data, int destination);
    void recordFilterChange(const QString& source, const QString& dest);
    void recordLogClear();
    void recordDeviceInteraction(const QString& deviceAddress, const QString& action);
    void recordUserWait(int durationMs);
    void recordUserComment(const QString& comment);

signals:
    void recordingStarted(const QString& testName);
    void recordingStopped();
    void actionRecorded(const QString& description);

private:
    struct RecordedAction {
        QString type;
        QVariantMap parameters;
        qint64 timestamp;
        QString description;
    };
    
    DeviceMainWindow* m_mainWindow;
    bool m_recording;
    QString m_testName;
    QList<RecordedAction> m_actions;
    qint64 m_recordingStartTime;
    qint64 m_lastActionTime;
    
    void addAction(const QString& type, const QVariantMap& params, const QString& desc);
    QString formatTimestamp(qint64 timestamp) const;
    int calculateWaitTime() const;
};
```

##### 3.1.2 Integration Points

The recorder hooks into existing UI components to capture user actions:

```cpp
// In DeviceMainWindow - PGN Dialog integration
void DeviceMainWindow::sendPGNMessage(int pgn, const QString& data, int dest) {
    // Existing send logic...
    sendMessage(pgn, data, dest);
    
    // Record the action if recording is active
    if (m_actionRecorder && m_actionRecorder->isRecording()) {
        m_actionRecorder->recordPGNSent(pgn, data, dest);
    }
}

// In PGNLogDialog - Filter changes
void PGNLogDialog::setSourceFilter(const QString& source) {
    // Existing filter logic...
    applySourceFilter(source);
    
    // Record filter change
    if (m_actionRecorder && m_actionRecorder->isRecording()) {
        m_actionRecorder->recordFilterChange(source, getCurrentDestFilter());
    }
}
```

##### 3.1.3 Generated Script Examples

**JavaScript Output:**
```javascript
// Generated test script: "Device Lighting Test"
// Recorded on: 2025-09-17 14:30:15
// Duration: 45.2 seconds

function deviceLightingTest() {
    startTest("Device Lighting Test");
    log("Recorded test - Device Lighting Test");
    
    try {
        // Clear message log (recorded at 0.0s)
        clearLog();
        
        // Send PGN 130561 to device 0E (recorded at 2.3s)
        sendPGN(130561, "00 FF 00 00 64 01 00 FF", 0x0E);
        log("Zone 1 ON command sent");
        
        // Wait for user interaction (recorded delay: 1.8s)
        waitMs(1800);
        
        // Send PGN 130561 to device 0E (recorded at 4.1s)
        sendPGN(130561, "00 FF 00 00 32 02 00 FF", 0x0E);
        log("Zone 2 50% brightness command sent");
        
        // User added comment: "Testing zone 2 dimming"
        log("Testing zone 2 dimming");
        
        // Wait for user interaction (recorded delay: 3.2s)
        waitMs(3200);
        
        // Send PGN 130561 to device 0E (recorded at 7.3s)
        sendPGN(130561, "00 FF 00 00 00 FF 00 FF", 0x0E);
        log("All zones OFF command sent");
        
        log("SUCCESS: Device lighting test completed");
        endTest(true);
        
    } catch (error) {
        log("FAIL: " + error);
        endTest(false);
    }
}
```

**JSON Output:**
```json
{
  "test": {
    "name": "Device Lighting Test",
    "description": "Recorded user interaction test",
    "recorded_date": "2025-09-17T14:30:15Z",
    "duration_seconds": 45.2,
    "timeout_seconds": 60,
    "steps": [
      {
        "action": "clear_log",
        "timestamp": "0.0s",
        "description": "Clear message log"
      },
      {
        "action": "send_pgn",
        "timestamp": "2.3s",
        "pgn": 130561,
        "data": "00 FF 00 00 64 01 00 FF",
        "destination": "0E",
        "description": "Zone 1 ON command sent"
      },
      {
        "action": "wait",
        "timestamp": "2.3s",
        "duration_ms": 1800,
        "description": "Wait for user interaction"
      },
      {
        "action": "send_pgn",
        "timestamp": "4.1s",
        "pgn": 130561,
        "data": "00 FF 00 00 32 02 00 FF",
        "destination": "0E",
        "description": "Zone 2 50% brightness command sent"
      },
      {
        "action": "user_comment",
        "timestamp": "4.1s",
        "message": "Testing zone 2 dimming"
      },
      {
        "action": "wait",
        "timestamp": "4.1s",
        "duration_ms": 3200,
        "description": "Wait for user interaction"
      },
      {
        "action": "send_pgn",
        "timestamp": "7.3s",
        "pgn": 130561,
        "data": "00 FF 00 00 00 FF 00 FF",
        "destination": "0E",
        "description": "All zones OFF command sent"
      }
    ]
  }
}
```

##### 3.1.4 Recording UI Components

**Recording Control Panel:**
```cpp
class RecordingControlWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RecordingControlWidget(ActionRecorder* recorder, QWidget *parent = nullptr);

private slots:
    void onStartRecording();
    void onStopRecording();
    void onPauseRecording();
    void onAddComment();
    void onSaveScript();
    void onPreviewScript();

private:
    ActionRecorder* m_recorder;
    QPushButton* m_startBtn;
    QPushButton* m_stopBtn;
    QPushButton* m_pauseBtn;
    QPushButton* m_commentBtn;
    QLabel* m_statusLabel;
    QLabel* m_timerLabel;
    QLineEdit* m_testNameEdit;
    QTimer* m_updateTimer;
    
    void updateRecordingStatus();
    void updateTimer();
};
```

**Recording Features:**
- **Visual Indicators** - Red recording dot, elapsed time display
- **Action Preview** - Real-time list of captured actions
- **Pause/Resume** - Temporarily pause recording during setup
- **Add Comments** - Insert descriptive comments during recording
- **Live Preview** - See generated script in real-time

#### 3.2 Test Suite Management

- **Test Discovery** - Automatic detection of test files
- **Test Organization** - Grouping and categorization
- **Parallel Execution** - Multiple test suites simultaneously
- **Dependency Management** - Test ordering and prerequisites

#### 3.3 Reporting and Analytics

- **Detailed Reports** - Test results with timing and message logs
- **Export Formats** - JSON, XML, CSV, HTML reports
- **Historical Tracking** - Test result database
- **Performance Metrics** - Message timing, response rates

#### 3.3 UI Integration

- **Script Editor** - Syntax highlighting, auto-completion
- **Test Runner** - Interactive test execution
- **Result Viewer** - Real-time test progress and results
- **Debugging Tools** - Breakpoints, step execution
- **Record/Playback** - Capture user actions into editable scripts

### Phase 4: CI/CD Integration

#### 4.1 Command-Line Interface

```bash
# Execute test suite
./pocotest --script test_suite.json --interface vcan0 --output results.json

# Run specific test
./pocotest --test "Device Discovery" --interface can0 --verbose

# Batch execution
./pocotest --batch tests/ --format junit --output test_results/
```

#### 4.2 Integration Options

- **Jenkins Integration** - Plugin for Jenkins CI/CD pipelines
- **GitHub Actions** - Workflow templates for automated testing
- **Docker Support** - Containerized test execution
- **API Endpoints** - REST API for external tool integration

## Use Cases and Applications

### Record/Playback Workflow Examples

**1. Customer Issue Reproduction**
```
Customer reports: "When I turn on zone 2 and then zone 4, zone 2 dims unexpectedly"

Workflow:
1. Start recording "Customer_Issue_Zone_Dimming"
2. Perform exact customer actions
3. Stop recording
4. Review generated script
5. Add assertions to verify expected behavior
6. Use script for regression testing
```

**2. Training New Users**
```
Expert user demonstrates optimal device setup procedure:

1. Record "Optimal_Device_Setup_Procedure"
2. Expert performs complete setup workflow
3. Generated script becomes training material
4. New users can replay exact steps
5. Script validates they performed steps correctly
```

**3. Manufacturing Test Creation**
```
QA engineer develops production test:

1. Record "Production_Line_Test"
2. Manually test all device functions
3. Add timing constraints and assertions
4. Script becomes automated production test
5. Manufacturing line runs identical test on every unit
```

### Development Testing

1. **Unit Testing** - Individual component functionality
2. **Integration Testing** - Device communication protocols
3. **Regression Testing** - Verify fixes don't break existing functionality
4. **Performance Testing** - Message timing and throughput

### Production Validation

1. **Manufacturing Test** - Quality assurance during production
2. **Field Testing** - Installation and commissioning verification
3. **Compliance Testing** - NMEA2000 standard compliance
4. **Interoperability Testing** - Multi-vendor device compatibility

### Research and Development

1. **Protocol Analysis** - NMEA2000 message behavior analysis
2. **Feature Development** - New feature validation
3. **Bug Reproduction** - Systematic issue reproduction
4. **Performance Optimization** - System optimization validation

## Technical Requirements

### Dependencies

- **Qt6** - Core application framework (already present)
- **QJSEngine** - JavaScript execution engine (Qt component)
- **NMEA2000** - Message handling libraries (already present)

### System Requirements

- **Linux** - Primary development and testing platform
- **CAN Interface** - Physical or virtual CAN bus access
- **Memory** - Additional ~10MB for script engine
- **Storage** - Test scripts and results storage

### Performance Considerations

- **Script Execution** - Minimal impact on real-time message processing
- **Memory Usage** - Efficient script engine with cleanup
- **Threading** - Non-blocking script execution
- **Resource Management** - Automatic cleanup of test resources

## Implementation Timeline

### Phase 1: Foundation (4-6 weeks)
- Core TestScriptEngine class implementation
- Basic JavaScript API
- Simple test execution
- UI integration for script loading

### Phase 2: Enhancement (3-4 weeks)
- JSON test suite support
- Advanced API functions
- Error handling and reporting
- Test result management

### Phase 3: Advanced Features (4-6 weeks)
- UI script editor
- Test suite management
- Comprehensive reporting
- Performance optimization

### Phase 4: Integration (2-3 weeks)
- Command-line interface
- CI/CD integration tools
- Documentation and examples
- User training materials

## Benefits and ROI

### Development Efficiency
- **Automated Testing** - Reduce manual testing time by 80%
- **Regression Prevention** - Catch issues before release
- **Consistent Testing** - Eliminate human error in testing
- **Documentation** - Self-documenting test procedures
- **Record/Playback** - Create tests by simply using the application

### Workflow Benefits
- **Learn by Doing** - Record actual workflows to understand best practices
- **Test Creation Speed** - Generate complex tests in minutes instead of hours
- **Knowledge Transfer** - Capture expert user workflows for training
- **Regression Testing** - Easily replay exact user scenarios that previously failed
- **Consistency** - Ensure tests are performed exactly the same way every time

### Quality Assurance
- **Comprehensive Coverage** - Test scenarios impossible manually
- **Reproducible Results** - Consistent test execution
- **Edge Case Testing** - Systematic boundary condition testing
- **Compliance Verification** - Automated standard compliance

### Business Value
- **Faster Development** - Rapid iteration and validation
- **Reduced Support** - Fewer field issues
- **Customer Confidence** - Demonstrable quality assurance
- **Competitive Advantage** - Superior testing capabilities

## Risk Assessment and Mitigation

### Technical Risks

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| Script Performance | Medium | Low | Efficient engine design, performance monitoring |
| Memory Leaks | High | Medium | Careful resource management, automated cleanup |
| API Stability | Medium | Low | Versioned API design, backward compatibility |
| Threading Issues | High | Low | Single-threaded script execution design |

### Operational Risks

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| User Adoption | Medium | Medium | Comprehensive documentation, training |
| Maintenance Overhead | Medium | Low | Modular design, automated testing |
| Feature Creep | Low | High | Phased implementation, clear requirements |

## Conclusion

The scripting feature represents a significant enhancement to the Lumitec Poco Tester that would:

1. **Transform** the tool from manual testing to automated validation
2. **Enable** comprehensive CI/CD integration for development workflows
3. **Provide** a competitive advantage in device testing capabilities
4. **Support** both development and production use cases
5. **Leverage** existing application capabilities efficiently

The proposed JavaScript-based approach provides the optimal balance of:
- **Ease of Use** - Familiar syntax and comprehensive API
- **Integration** - Seamless integration with existing Qt application
- **Extensibility** - Foundation for future enhancements
- **Performance** - Minimal impact on application performance

This feature would position the Lumitec Poco Tester as a comprehensive testing platform suitable for development, production, and field deployment scenarios.

## Next Steps

1. **Prototype Implementation** - Develop core TestScriptEngine class
2. **API Design Review** - Validate JavaScript API with test scenarios
3. **UI Mockups** - Design script editor and test runner interfaces
4. **Performance Testing** - Validate script execution performance
5. **User Feedback** - Gather requirements from potential users

The implementation should begin with Phase 1 (Foundation) to establish the core capabilities and validate the approach before proceeding with advanced features.
#ifndef DEVICEMAINWINDOW_H
#define DEVICEMAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QTimer>
#include <QHeaderView>
#include <QLabel>
#include <QComboBox>
#include <QMap>
#include <QSet>
#include <QMenuBar>
#include <QStatusBar>
#include <QDateTime>
#include <N2kMsg.h>
#include <N2kDeviceList.h>
#include "LumitecPoco.h"
#include "instanceconflictanalyzer.h"

// Forward declarations
class tN2kDeviceList;
class tNMEA2000_SocketCAN;
class PGNLogDialog;
class PocoDeviceDialog;
class InstanceConflictAnalyzer;

class DeviceMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit DeviceMainWindow(QWidget *parent = nullptr);
    ~DeviceMainWindow();
    
    void initNMEA2000();
    void timerEvent(QTimerEvent *event) override;
    
    // Conflict analysis interface
    bool hasInstanceConflicts() const;
    int getConflictCount() const;
    
    // Device name resolution
    QString getDeviceName(uint8_t deviceAddress) const;

private slots:
    void updateDeviceList();
    void onConnectClicked();
    void onDisconnectClicked();
    void onRefreshClicked();
    void onRowSelectionChanged();
    void analyzeInstanceConflicts();
    void showPGNLog();
    void showSendPGNDialog();
    void onCanInterfaceChanged(const QString &interface);
    void clearConflictHistory();
    void showDeviceContextMenu(const QPoint& position);

private:
    void setupUI();
    void setupMenuBar();
    void populateDeviceTable();
    void highlightInstanceConflicts();
    QString getDeviceClassName(unsigned char deviceClass);
    QString getDeviceFunctionName(unsigned char deviceFunction);
    QString getManufacturerName(uint16_t manufacturerCode);
    QString getPGNName(unsigned long pgn);
    
    // NMEA2000 and PGN handling
    void handleN2kMsg(const tN2kMsg& msg);
    static void staticN2kMsgHandler(const tN2kMsg &msg);
    static DeviceMainWindow* instance; // Singleton-style reference for static callback
    
    void setupCanInterfaceSelector();
    void populateCanInterfaces();
    void reinitializeNMEA2000();
    QStringList getAvailableCanInterfaces();
    void updateConnectionButtonStates();
    void verifyCanInterface();
#ifdef ENABLE_IPG100_SUPPORT
    void addManualIPG100();
#endif
    
    // Device activity tracking methods
    void updateDeviceActivity(uint8_t sourceAddress);
    void checkDeviceTimeouts();
    void grayOutInactiveDevices();
    void updateDeviceTableRow(int row, uint8_t source, const tNMEA2000::tDevice* device, bool isActive);
    
    // Context menu methods
    void showSendPGNToDevice(uint8_t targetAddress, const QString& nodeAddress);
    void showDeviceDetails(int row);
    void queryDeviceConfiguration(uint8_t targetAddress);
    void requestProductInformation(uint8_t targetAddress);
    void requestSupportedPGNs(uint8_t targetAddress);
    void requestAllInformation(uint8_t targetAddress);
    void requestInfoFromAllDevices();
    void triggerAutomaticDeviceDiscovery();
    void showPGNLogForDevice(uint8_t sourceAddress);
    
    // Lumitec Poco message handling
    void handleLumitecPocoMessage(const tN2kMsg& msg);
    void handleProductInformationResponse(const tN2kMsg& msg);
    void handleConfigurationInformationResponse(const tN2kMsg& msg);
    void handleGroupFunctionMessage(const tN2kMsg& msg);
    void displayLumitecMessage(const tN2kMsg& msg, const QString& description);
    
    // Lumitec Poco control methods
    void sendLumitecSimpleAction(uint8_t targetAddress, uint8_t actionId, uint8_t switchId);
    void showLumitecColorDialog(uint8_t targetAddress, const QString& nodeAddress);
    void showLumitecSwitchActionDialog(uint8_t targetAddress, const QString& nodeAddress);
    void sendLumitecCustomHSB(uint8_t targetAddress, uint8_t hue, uint8_t saturation, uint8_t brightness);
    void showPocoDeviceDialog(uint8_t targetAddress, const QString& nodeAddress);
    void sendZonePGN130561(uint8_t targetAddress, uint8_t zoneId, const QString& zoneName,
                          uint8_t red, uint8_t green, uint8_t blue, uint16_t colorTemp,
                          uint8_t intensity, uint8_t programId, uint8_t programColorSeqIndex,
                          uint8_t programIntensity, uint8_t programRate,
                          uint8_t programColorSequence, bool zoneEnabled);
    
    // Activity indicator methods
    void setupActivityIndicators();
    void blinkTxIndicator();
    void blinkRxIndicator();

private slots:
    // Activity indicator slots
    void onTxBlinkTimeout();
    void onRxBlinkTimeout();
    
    // Poco device dialog slots
    void onPocoSwitchActionRequested(uint8_t deviceAddress, uint8_t switchId, uint8_t actionId);
    void onPocoColorControlRequested(uint8_t deviceAddress);
    void onPocoDeviceInfoRequested(uint8_t deviceAddress);
    void onPocoZoneLightingControlRequested(uint8_t deviceAddress);
    public slots:
    void onZonePGN130561Requested(uint8_t deviceAddress, uint8_t zoneId, const QString& zoneName,
                                 uint8_t red, uint8_t green, uint8_t blue, uint16_t colorTemp,
                                 uint8_t intensity, uint8_t programId, uint8_t programColorSeqIndex,
                                 uint8_t programIntensity, uint8_t programRate,
                                 uint8_t programColorSequence, bool zoneEnabled);

signals:
    void commandAcknowledged(uint8_t deviceAddress, uint32_t pgn, bool success);

private:
    // UI Components
    QWidget* m_centralWidget;
    QTableWidget* m_deviceTable;
    QPushButton* m_refreshButton;
    QPushButton* m_analyzeButton;
    QPushButton* m_pgnLogButton;
    QPushButton* m_sendPGNButton;
    QPushButton* m_clearConflictsButton;
    QLabel* m_statusLabel;
    QTimer* m_updateTimer;
    QComboBox* m_canInterfaceCombo;
    
    // Activity indicators
    QLabel* m_txIndicator;
    QLabel* m_rxIndicator;
    QTimer* m_txBlinkTimer;
    QTimer* m_rxBlinkTimer;
    
    // NMEA2000 Components
    tN2kDeviceList* m_deviceList;
    QPushButton *m_connectButton = nullptr;
    QPushButton *m_disconnectButton = nullptr;
    QCheckBox *m_autoDiscoveryCheckbox = nullptr;
    tNMEA2000 *m_nmea2000Instance = nullptr;
    QString m_currentInterface;
    bool m_isConnected;
    
    // Secondary dialogs
    PGNLogDialog* m_pgnLogDialog;
    
    // Instance conflict analysis
    InstanceConflictAnalyzer* m_conflictAnalyzer;
    
    // Device activity tracking
    struct DeviceActivity {
        QDateTime lastSeen;
        bool isActive;
        int tableRow; // Track table row to avoid reordering
    };
    QMap<uint8_t, DeviceActivity> m_deviceActivity; // key: source address
    static const int DEVICE_TIMEOUT_MS = 30000; // 30 seconds
    
    // Product information request tracking
    QSet<uint8_t> m_pendingProductInfoRequests; // Track which devices we've requested info from
    QSet<uint8_t> m_pendingConfigInfoRequests; // Track which devices we've requested config info from
    
    // New device detection tracking
    QSet<uint8_t> m_knownDevices; // Track devices we've seen before
    
    // Automatic device discovery tracking
    bool m_hasSeenValidTraffic;
    bool m_autoDiscoveryTriggered;
    QDateTime m_interfaceStartTime;
    int m_messagesReceived;
    static const int AUTO_DISCOVERY_DELAY_MS = 5000; // Wait 5 seconds after first traffic
    static const int MIN_MESSAGES_FOR_DISCOVERY = 10; // Require at least 10 messages
    
    // Follow-up device query tracking
    bool m_followUpQueriesScheduled;
    static const int FOLLOWUP_QUERY_DELAY_MS = 5000; // Wait 5 seconds after auto-discovery
    
    // Helper methods
    void updatePGNDialogDeviceList();
    void scheduleFollowUpQueries();
    void performFollowUpQueries();
    void queryNewDevice(uint8_t sourceAddress);
    
};

#endif // DEVICEMAINWINDOW_H

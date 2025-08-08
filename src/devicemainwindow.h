#ifndef DEVICEMAINWINDOW_H
#define DEVICEMAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTimer>
#include <QHeaderView>
#include <QLabel>
#include <QComboBox>
#include <QMap>
#include <QSet>
#include <QMenuBar>
#include <QStatusBar>
#include <N2kMsg.h>
#include <N2kDeviceList.h>

// Forward declarations
class tN2kDeviceList;
class tNMEA2000_SocketCAN;
class PGNLogDialog;

// Structure to track PGN instance data
struct PGNInstanceData {
    unsigned long pgn;
    uint8_t source;
    uint8_t instance;
    qint64 lastSeen;  // timestamp
};

// Structure to track conflicts
struct InstanceConflict {
    unsigned long pgn;
    uint8_t instance;
    QSet<uint8_t> conflictingSources;
};

class DeviceMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit DeviceMainWindow(QWidget *parent = nullptr);
    ~DeviceMainWindow();
    
    void initNMEA2000();
    void timerEvent(QTimerEvent *event) override;
    
    // PGN instance conflict tracking
    QList<InstanceConflict> getInstanceConflicts() const;
    bool hasInstanceConflicts() const;
    QSet<uint8_t> getConflictingSources() const;

private slots:
    void updateDeviceList();
    void onRefreshClicked();
    void onRowSelectionChanged();
    void analyzeInstanceConflicts();
    void showPGNLog();
    void showSendPGNDialog();
    void onCanInterfaceChanged(const QString &interface);
    void clearConflictHistory();

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
    
    // PGN instance tracking methods
    void trackPGNInstance(const tN2kMsg& msg);
    uint8_t extractInstanceFromPGN(const tN2kMsg& msg);
    void updateInstanceConflicts();
    bool isPGNWithInstance(unsigned long pgn);

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
    
    // NMEA2000 Components
    tN2kDeviceList* m_deviceList;
    QString m_currentInterface;
    
    // PGN instance tracking
    QMap<QString, PGNInstanceData> m_pgnInstances; // key: "pgn_source" -> instance data
    QList<InstanceConflict> m_instanceConflicts;
    QSet<uint8_t> m_conflictingSources;
    
    // Secondary dialogs
    PGNLogDialog* m_pgnLogDialog;
};

#endif // DEVICEMAINWINDOW_H

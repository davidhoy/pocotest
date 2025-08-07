#include <QMainWindow>
#include <QComboBox>
#include <QMap>
#include <QSet>
#include <N2kMsg.h>
#include <N2kDeviceList.h>
#include "ui_mainwindow.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

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

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void initNMEA2000();
    void timerEvent(QTimerEvent *event) override;
    tN2kDeviceList* getDeviceList() const { return m_deviceList; }
    
    // PGN instance conflict tracking
    QList<InstanceConflict> getInstanceConflicts() const;
    bool hasInstanceConflicts() const;
    QSet<uint8_t> getConflictingSources() const;

private slots:
    void on_sendButton_clicked();
    void on_sendPGNButton_clicked();
    void on_canInterfaceChanged(const QString &interface);
    void clearLog();
    void showDeviceList();

private:
    void handleN2kMsg(const tN2kMsg& msg);
    static void staticN2kMsgHandler(const tN2kMsg &msg);
    static MainWindow* instance; // Singleton-style reference for static callback
    
    void setupCanInterfaceSelector();
    void setupMainWindowProperties();
    void populateCanInterfaces();
    void reinitializeNMEA2000();
    QStringList getAvailableCanInterfaces();
    
    // PGN instance tracking methods
    void trackPGNInstance(const tN2kMsg& msg);
    uint8_t extractInstanceFromPGN(const tN2kMsg& msg);
    void updateInstanceConflicts();
    bool isPGNWithInstance(unsigned long pgn);

private:
    Ui::MainWindow *ui;
    QComboBox* m_canInterfaceCombo;
    QString m_currentInterface;
    tN2kDeviceList* m_deviceList;
    
    // PGN instance tracking
    QMap<QString, PGNInstanceData> m_pgnInstances; // key: "pgn_source" -> instance data
    QList<InstanceConflict> m_instanceConflicts;
    QSet<uint8_t> m_conflictingSources;
};

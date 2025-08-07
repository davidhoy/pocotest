#include <QMainWindow>
#include <QComboBox>
#include <N2kMsg.h>
#include <N2kDeviceList.h>
#include "ui_mainwindow.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void initNMEA2000();
    void timerEvent(QTimerEvent *event) override;
    tN2kDeviceList* getDeviceList() const { return m_deviceList; }

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

private:
    Ui::MainWindow *ui;
    QComboBox* m_canInterfaceCombo;
    QString m_currentInterface;
    tN2kDeviceList* m_deviceList;
};

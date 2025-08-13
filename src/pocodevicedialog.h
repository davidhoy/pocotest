#ifndef POCODEVICEDIALOG_H
#define POCODEVICEDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QComboBox>
#include <QGroupBox>
#include <QTabWidget>
#include <QDialogButtonBox>

class PocoDeviceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PocoDeviceDialog(uint8_t deviceAddress, const QString& deviceName, QWidget *parent = nullptr);
    ~PocoDeviceDialog();

private slots:
    void onSwitchActionTriggered();
    void onColorControlTriggered();
    void onDeviceInfoRequested();

private:
    void setupUI();
    void setupSwitchControlTab();
    void setupColorControlTab();
    void setupDeviceInfoTab();

    // Device information
    uint8_t m_deviceAddress;
    QString m_deviceName;

    // UI Components
    QTabWidget* m_tabWidget;
    
    // Switch Control Tab
    QWidget* m_switchControlTab;
    QSpinBox* m_switchIdSpinBox;
    QComboBox* m_actionComboBox;
    QPushButton* m_sendActionButton;
    
    // Color Control Tab
    QWidget* m_colorControlTab;
    QPushButton* m_colorControlButton;
    
    // Device Info Tab
    QWidget* m_deviceInfoTab;
    QLabel* m_deviceAddressLabel;
    QLabel* m_deviceNameLabel;
    QPushButton* m_queryDeviceButton;
    
    // Dialog buttons
    QDialogButtonBox* m_buttonBox;

signals:
    void switchActionRequested(uint8_t deviceAddress, uint8_t switchId, uint8_t actionId);
    void colorControlRequested(uint8_t deviceAddress);
    void deviceInfoRequested(uint8_t deviceAddress);
};

#endif // POCODEVICEDIALOG_H

#ifndef DEVICELISTDIALOG_H
#define DEVICELISTDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTimer>
#include <QHeaderView>
#include <QLabel>
#include <N2kDeviceList.h>

class DeviceListDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DeviceListDialog(QWidget *parent = nullptr);
    ~DeviceListDialog();

private slots:
    void updateDeviceList();
    void onRefreshClicked();
    void onCloseClicked();

private:
    void setupUI();
    void populateDeviceTable();
    QString getDeviceClassName(unsigned long deviceClass);
    QString getDeviceFunctionName(unsigned char deviceFunction);

private:
    QTableWidget* m_deviceTable;
    QPushButton* m_refreshButton;
    QPushButton* m_closeButton;
    QLabel* m_statusLabel;
    QTimer* m_updateTimer;
};

#endif // DEVICELISTDIALOG_H

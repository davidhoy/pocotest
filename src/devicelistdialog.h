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

// Forward declaration
class tN2kDeviceList;

class DeviceListDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DeviceListDialog(QWidget *parent = nullptr, tN2kDeviceList* deviceList = nullptr);
    ~DeviceListDialog();

private slots:
    void updateDeviceList();
    void onRefreshClicked();
    void onCloseClicked();
    void onRowSelectionChanged();

private:
    void setupUI();
    void populateDeviceTable();
    void analyzeInstanceConflicts();
    void highlightInstanceConflicts();
    QString getDeviceClassName(unsigned char deviceClass);
    QString getDeviceFunctionName(unsigned char deviceFunction);
    QString getManufacturerName(uint16_t manufacturerCode);

private:
    QTableWidget* m_deviceTable;
    QPushButton* m_refreshButton;
    QPushButton* m_closeButton;
    QLabel* m_statusLabel;
    QTimer* m_updateTimer;
    tN2kDeviceList* m_deviceList;
    
    // Conflict tracking
    QMap<uint8_t, QList<uint8_t>> m_conflictGroups; // source -> list of conflicting sources
    QSet<uint8_t> m_conflictingSources; // all sources that have conflicts
};

#endif // DEVICELISTDIALOG_H

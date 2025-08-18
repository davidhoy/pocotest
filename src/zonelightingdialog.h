#ifndef ZONELIGHTINGDIALOG_H
#define ZONELIGHTINGDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QLineEdit>
#include <QSlider>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QTimer>
#include <QCheckBox>
#include <QTabWidget>
#include <cstdint>

class ZoneLightingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ZoneLightingDialog(uint8_t deviceAddress, const QString& deviceName, QWidget *parent = nullptr);
    ~ZoneLightingDialog();

private slots:
    void onSendSingleZone();
    void onSendMultipleZones();
    void onSendAllZonesOn();
    void onSendAllZonesOff();
    void sendNextZoneInSequence();
    void onColorSliderChanged();
    void updateColorPreview();

private:
    void setupUI();
    void setupSingleZoneTab();
    void setupMultiZoneTab();
    void createColorControls(QVBoxLayout* layout);
    void createProgramControls(QVBoxLayout* layout);
    void sendZonePGN130561(uint8_t zoneId, const QString& zoneName, 
                          uint8_t red, uint8_t green, uint8_t blue,
                          uint16_t colorTemp, uint8_t intensity,
                          uint8_t programId, uint8_t programColorSeqIndex,
                          uint8_t programIntensity, uint8_t programRate,
                          uint8_t programColorSequence, bool zoneEnabled);

    // Device information
    uint8_t m_deviceAddress;
    QString m_deviceName;

    // UI Components
    QTabWidget* m_tabWidget;
    QDialogButtonBox* m_buttonBox;
    
    // Single Zone Tab
    QWidget* m_singleZoneTab;
    QSpinBox* m_zoneIdSpinBox;
    QLineEdit* m_zoneNameEdit;
    QSlider* m_redSlider;
    QSlider* m_greenSlider;
    QSlider* m_blueSlider;
    QLabel* m_redValueLabel;
    QLabel* m_greenValueLabel;
    QLabel* m_blueValueLabel;
    QLabel* m_colorPreviewLabel;
    QSpinBox* m_colorTempSpinBox;
    QSlider* m_intensitySlider;
    QLabel* m_intensityValueLabel;
    QSpinBox* m_programIdSpinBox;
    QSpinBox* m_programColorSeqIndexSpinBox;
    QSlider* m_programIntensitySlider;
    QSlider* m_programRateSlider;
    QSlider* m_programColorSequenceSlider;
    QLabel* m_programIntensityValueLabel;
    QLabel* m_programRateValueLabel;
    QLabel* m_programColorSequenceValueLabel;
    QCheckBox* m_zoneEnabledCheckBox;
    QPushButton* m_sendSingleZoneButton;
    
    // Multi Zone Tab
    QWidget* m_multiZoneTab;
    QSpinBox* m_startZoneSpinBox;
    QSpinBox* m_endZoneSpinBox;
    QSpinBox* m_delaySpinBox;
    QCheckBox* m_waitForAckCheckBox;
    QCheckBox* m_retryOnTimeoutCheckBox;
    QPushButton* m_sendMultipleZonesButton;
    QPushButton* m_sendAllZonesOnButton;
    QPushButton* m_sendAllZonesOffButton;
    
    // Multi-zone sequence state
    QTimer* m_sequenceTimer;
    int m_currentZoneInSequence;
    int m_endZoneInSequence;
    bool m_isOnSequence; // true for ON, false for OFF
    bool m_waitingForAcknowledgment; // true when waiting for ACK before sending next command
    QTimer* m_acknowledgmentTimeoutTimer; // timeout for acknowledgment
    int m_currentRetryCount; // current retry attempt for the current command
    int m_maxRetries; // maximum number of retries (default 3)

public slots:
    void onCommandAcknowledged(uint8_t deviceAddress, uint32_t pgn, bool success);

private slots:
    void onAcknowledgmentTimeout();
    
private:
    void sendCurrentZoneCommand(); // Helper to send the current zone command

signals:
    void zonePGN130561Requested(uint8_t deviceAddress, uint8_t zoneId, const QString& zoneName,
                               uint8_t red, uint8_t green, uint8_t blue, uint16_t colorTemp,
                               uint8_t intensity, uint8_t programId, uint8_t programColorSeqIndex,
                               uint8_t programIntensity, uint8_t programRate,
                               uint8_t programColorSequence, bool zoneEnabled);
};

#endif // ZONELIGHTINGDIALOG_H

#ifndef DIRECTCHANNELCONTROLDIALOG_H
#define DIRECTCHANNELCONTROLDIALOG_H

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
#include <QTabWidget>
#include <QComboBox>
#include <QCheckBox>
#include <cstdint>

class DirectChannelControlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DirectChannelControlDialog(uint8_t deviceAddress, const QString& deviceName, QWidget *parent = nullptr);
    ~DirectChannelControlDialog();

private slots:
    void onSendBinControl();
    void onSendPwmControl();
    void onSendPliControl();
    void onSendPliT2HSBControl();
    void onChannelTypeChanged(int index);
    void updatePliHexDisplay();

private:
    void setupUI();
    void setupBinTab();
    void setupPwmTab();
    void setupPliTab();
    void setupPliT2HSBTab();

    // Device information
    uint8_t m_deviceAddress;
    QString m_deviceName;

    // UI Components
    QTabWidget* m_tabWidget;
    QDialogButtonBox* m_buttonBox;

    // BIN Control Tab
    QWidget* m_binTab;
    QSpinBox* m_binChannelSpinBox;
    QComboBox* m_binStateComboBox;
    QPushButton* m_sendBinButton;

    // PWM Control Tab
    QWidget* m_pwmTab;
    QSpinBox* m_pwmChannelSpinBox;
    QSlider* m_pwmDutySlider;
    QLabel* m_pwmDutyValueLabel;
    QSpinBox* m_pwmTransitionTimeSpinBox;
    QPushButton* m_sendPwmButton;

    // PLI Control Tab
    QWidget* m_pliTab;
    QSpinBox* m_pliChannelSpinBox;
    QLineEdit* m_pliMessageHexEdit;
    QLineEdit* m_pliMessageDecEdit;
    QPushButton* m_sendPliButton;

    // PLI T2HSB Control Tab
    QWidget* m_pliT2HSBTab;
    QSpinBox* m_pliT2HSBChannelSpinBox;
    QSpinBox* m_pliClanSpinBox;
    QSpinBox* m_pliTransitionSpinBox;
    QSlider* m_pliBrightnessSlider;
    QLabel* m_pliBrightnessValueLabel;
    QSlider* m_pliHueSlider;
    QLabel* m_pliHueValueLabel;
    QSlider* m_pliSaturationSlider;
    QLabel* m_pliSaturationValueLabel;
    QPushButton* m_sendPliT2HSBButton;

signals:
    void binControlRequested(uint8_t deviceAddress, uint8_t channel, uint8_t state);
    void pwmControlRequested(uint8_t deviceAddress, uint8_t channel, uint8_t duty, uint16_t transitionTime);
    void pliControlRequested(uint8_t deviceAddress, uint8_t channel, uint32_t pliMessage);
    void pliT2HSBControlRequested(uint8_t deviceAddress, uint8_t channel, uint8_t pliClan,
                                 uint8_t transition, uint8_t brightness, uint8_t hue, uint8_t saturation);
};

#endif // DIRECTCHANNELCONTROLDIALOG_H
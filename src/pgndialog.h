#ifndef PGNDIALOG_H
#define PGNDIALOG_H

#include <QDialog>
#include <QSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QGroupBox>
#include <QTextEdit>
#include <QSplitter>
#include <N2kMsg.h>

class PGNDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PGNDialog(QWidget *parent = nullptr);
    void setDestinationAddress(uint8_t address);

signals:
    void messageTransmitted(const tN2kMsg& message);

private slots:
    void onSendPGN();
    void onPGNSelectionChanged();
    void onClearData();

private:
    void setupUI();
    void populateCommonPGNs();
    void updateDataFieldsForPGN(unsigned long pgn);
    tN2kMsg createMessageFromInputs();
    
    // UI Components
    QComboBox* m_pgnComboBox;
    QSpinBox* m_prioritySpinBox;
    QSpinBox* m_sourceSpinBox;
    QSpinBox* m_destinationSpinBox;
    QTextEdit* m_dataTextEdit;
    QTextEdit* m_previewTextEdit;
    QPushButton* m_sendButton;
    QPushButton* m_clearButton;
    QPushButton* m_cancelButton;
    
    // PGN-specific parameter widgets
    QWidget* m_parameterWidget;
    QFormLayout* m_parameterLayout;
    
    struct PGNInfo {
        unsigned long pgn;
        QString name;
        QString description;
        QStringList parameters;
    };
    
    QList<PGNInfo> m_commonPGNs;
    
    // Store the intended destination address separately from the spinbox
    uint8_t m_intendedDestination;
};

#endif // PGNDIALOG_H

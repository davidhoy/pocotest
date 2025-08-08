#ifndef PGNLOGDIALOG_H
#define PGNLOGDIALOG_H

#include <QDialog>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <N2kMsg.h>

class PGNLogDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PGNLogDialog(QWidget *parent = nullptr);
    ~PGNLogDialog();
    
    void appendMessage(const tN2kMsg& msg);

private slots:
    void clearLog();
    void onCloseClicked();

private:
    void setupUI();

private:
    QTextEdit* m_logTextEdit;
    QPushButton* m_clearButton;
    QPushButton* m_closeButton;
    QLabel* m_statusLabel;
};

#endif // PGNLOGDIALOG_H

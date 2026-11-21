#ifndef INITIALIZATIONDIALOG_H
#define INITIALIZATIONDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QTextEdit>
#include <QProcess>
#include <QPoint>

class InitializationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit InitializationDialog(QWidget *parent = nullptr);
    ~InitializationDialog() override;

    QString systemImageUrl() const;
    QString vendorImageUrl() const;

signals:
    void initializationSucceeded();
    void initializationFailed(const QString &errorMessage);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void handleStartInitialization();
    void handleProcessOutput();
    void handleProcessError();
    void handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void setupUi();
    void appendLog(const QString &text, const QString &color = QString());

    QLineEdit    *m_editSystemUrl = nullptr;
    QLineEdit    *m_editVendorUrl = nullptr;
    QLabel       *m_lblNotice = nullptr;
    QLabel       *m_lblStatus = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QTextEdit    *m_logConsole = nullptr;

    QPushButton  *m_btnInit = nullptr;
    QPushButton  *m_btnCancel = nullptr;

    QProcess     *m_process = nullptr;
    QPoint        m_dragPosition;
    bool          m_isRunning = false;
};

#endif // INITIALIZATIONDIALOG_H

#include "InitializationDialog.h"
#include "Theme.h"
#include "PathResolver.h"
#include "Logger.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPainter>
#include <QMouseEvent>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QTimer>
#include <QStandardPaths>
#include <QScrollBar>

static const QString DEFAULT_SYS_URL =
    "https://sourceforge.net/projects/waydroid/files/images/system/lineage/waydroid_x86_64/lineage-18.1-20250628-VANILLA-waydroid_x86_64-system.zip/download";
static const QString DEFAULT_VND_URL =
    "https://sourceforge.net/projects/waydroid/files/images/vendor/waydroid_x86_64/lineage-18.1-20250628-MAINLINE-waydroid_x86_64-vendor.zip/download";
static const QString HELPER_NOTICE_TEXT =
    "These are links for Waydroid Lineage OS Android 11. You can enter your own Android image links, or copy system.img and vendor.img into LinStack/resources (or /opt/linstack/resources before running Init).";

InitializationDialog::InitializationDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setStyleSheet(Theme::cyberEmeraldStyleSheet());

    resize(780, 560);
    setupUi();
}

InitializationDialog::~InitializationDialog()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(500);
    }
}

QString InitializationDialog::systemImageUrl() const
{
    return m_editSystemUrl ? m_editSystemUrl->text().trimmed() : DEFAULT_SYS_URL;
}

QString InitializationDialog::vendorImageUrl() const
{
    return m_editVendorUrl ? m_editVendorUrl->text().trimmed() : DEFAULT_VND_URL;
}

void InitializationDialog::setupUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(20, 20, 20, 20);
    rootLayout->setSpacing(14);

    // 1. Header Bar
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 4);

    auto *lblTitle = new QLabel("LinStack Container Initialization", this);
    QFont titleFont = lblTitle->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    lblTitle->setFont(titleFont);
    lblTitle->setStyleSheet("color: #00FF66; font-weight: bold;");

    headerLayout->addWidget(lblTitle);
    headerLayout->addStretch();

    auto *btnClose = new QPushButton("✕", this);
    btnClose->setFixedSize(28, 28);
    btnClose->setCursor(Qt::PointingHandCursor);
    btnClose->setStyleSheet("QPushButton { background-color: transparent; color: #9CA3AF; border: none; font-size: 14px; border-radius: 4px; }"
                            "QPushButton:hover { background-color: #EF4444; color: #FFFFFF; }");
    connect(btnClose, &QPushButton::clicked, this, &QDialog::reject);
    headerLayout->addWidget(btnClose);

    rootLayout->addLayout(headerLayout);

    // 2. Exact Helper Notice Banner
    m_lblNotice = new QLabel(HELPER_NOTICE_TEXT, this);
    m_lblNotice->setWordWrap(true);
    m_lblNotice->setStyleSheet(
        "QLabel {"
        "  background-color: #161F30;"
        "  color: #F9FAFB;"
        "  border-left: 4px solid #10B981;"
        "  border-radius: 4px;"
        "  padding: 10px 14px;"
        "  font-size: 12px;"
        "  line-height: 1.4;"
        "}"
    );
    rootLayout->addWidget(m_lblNotice);

    // 3. Image URLs Configuration Group
    auto *groupUrls = new QGroupBox("Container Image URLs", this);
    groupUrls->setStyleSheet(
        "QGroupBox {"
        "  color: #10B981;"
        "  font-weight: bold;"
        "  border: 1px solid #1F2937;"
        "  border-radius: 6px;"
        "  margin-top: 10px;"
        "  padding-top: 12px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 12px;"
        "  padding: 0 4px;"
        "}"
    );

    auto *formLayout = new QFormLayout(groupUrls);
    formLayout->setContentsMargins(14, 14, 14, 14);
    formLayout->setSpacing(10);

    auto *lblSys = new QLabel("System URL:", this);
    lblSys->setStyleSheet("color: #9CA3AF; font-size: 12px;");
    m_editSystemUrl = new QLineEdit(this);
    m_editSystemUrl->setText(DEFAULT_SYS_URL);
    m_editSystemUrl->setStyleSheet("QLineEdit { background-color: #0B0F19; color: #F9FAFB; border: 1px solid #1F2937; border-radius: 4px; padding: 6px; } QLineEdit:focus { border-color: #10B981; }");
    formLayout->addRow(lblSys, m_editSystemUrl);

    auto *lblVnd = new QLabel("Vendor URL:", this);
    lblVnd->setStyleSheet("color: #9CA3AF; font-size: 12px;");
    m_editVendorUrl = new QLineEdit(this);
    m_editVendorUrl->setText(DEFAULT_VND_URL);
    m_editVendorUrl->setStyleSheet("QLineEdit { background-color: #0B0F19; color: #F9FAFB; border: 1px solid #1F2937; border-radius: 4px; padding: 6px; } QLineEdit:focus { border-color: #10B981; }");
    formLayout->addRow(lblVnd, m_editVendorUrl);

    rootLayout->addWidget(groupUrls);

    // 4. Progress and Live Log
    m_lblStatus = new QLabel("Ready to initialize container.", this);
    m_lblStatus->setStyleSheet("color: #34D399; font-weight: bold; font-size: 12px;");
    rootLayout->addWidget(m_lblStatus);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFixedHeight(16);
    m_progressBar->setStyleSheet(
        "QProgressBar {"
        "  background-color: #0B0F19;"
        "  border: 1px solid #1F2937;"
        "  border-radius: 8px;"
        "  text-align: center;"
        "  color: #F9FAFB;"
        "  font-size: 10px;"
        "}"
        "QProgressBar::chunk {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #059669, stop:1 #00FF66);"
        "  border-radius: 7px;"
        "}"
    );
    rootLayout->addWidget(m_progressBar);

    m_logConsole = new QTextEdit(this);
    m_logConsole->setReadOnly(true);
    m_logConsole->setStyleSheet(
        "QTextEdit {"
        "  background-color: #0D121D;"
        "  color: #10B981;"
        "  font-family: monospace;"
        "  font-size: 11px;"
        "  border: 1px solid #1F2937;"
        "  border-radius: 6px;"
        "  padding: 8px;"
        "}"
    );
    rootLayout->addWidget(m_logConsole);

    // 5. Actions Footer
    auto *footerLayout = new QHBoxLayout();
    footerLayout->setContentsMargins(0, 6, 0, 0);

    m_btnCancel = new QPushButton("Cancel", this);
    m_btnCancel->setFixedWidth(110);
    m_btnCancel->setCursor(Qt::PointingHandCursor);
    m_btnCancel->setStyleSheet("QPushButton { background-color: #1E293B; color: #F9FAFB; border: 1px solid #374151; border-radius: 6px; padding: 8px; font-weight: bold; }"
                              "QPushButton:hover { background-color: #374151; }");
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    footerLayout->addWidget(m_btnCancel);

    footerLayout->addStretch();

    m_btnInit = new QPushButton("Init Container", this);
    m_btnInit->setFixedWidth(150);
    m_btnInit->setCursor(Qt::PointingHandCursor);
    m_btnInit->setStyleSheet("QPushButton { background-color: #10B981; color: #0B0F19; border: none; border-radius: 6px; padding: 8px; font-weight: bold; }"
                            "QPushButton:hover { background-color: #00FF66; }"
                            "QPushButton:disabled { background-color: #374151; color: #6B7280; }");
    connect(m_btnInit, &QPushButton::clicked, this, &InitializationDialog::handleStartInitialization);
    footerLayout->addWidget(m_btnInit);

    rootLayout->addLayout(footerLayout);
}

void InitializationDialog::appendLog(const QString &text, const QString &color)
{
    if (!m_logConsole) return;
    QString formatted = text;
    if (!color.isEmpty()) {
        formatted = QString("<span style=\"color:%1;\">%2</span>").arg(color, text.toHtmlEscaped());
        m_logConsole->append(formatted);
    } else {
        m_logConsole->append(text);
    }
    m_logConsole->verticalScrollBar()->setValue(m_logConsole->verticalScrollBar()->maximum());
}

void InitializationDialog::handleStartInitialization()
{
    if (m_isRunning) return;

    m_isRunning = true;
    m_btnInit->setEnabled(false);
    m_btnCancel->setEnabled(false);
    m_editSystemUrl->setEnabled(false);
    m_editVendorUrl->setEnabled(false);
    m_progressBar->setValue(5);

    QString resDir = PathResolver::resolveResourcesDir();
    QDir().mkpath(resDir);

    QString sysPath = resDir + "/system.img";
    QString vndPath = resDir + "/vendor.img";

    bool hasLocalSys = QFileInfo::exists(sysPath) && QFileInfo(sysPath).size() > 0;
    bool hasLocalVnd = QFileInfo::exists(vndPath) && QFileInfo(vndPath).size() > 0;

    if (hasLocalSys && hasLocalVnd) {
        m_lblStatus->setText("Found local images. Initializing container...");
        appendLog(QString("[Step A/B] Found local system.img and vendor.img in %1.").arg(resDir), "#34D399");
        appendLog("Skipping download; proceeding with local initialization.", "#34D399");
        m_progressBar->setValue(40);
    } else {
        m_lblStatus->setText("Local images missing. Downloading and extracting...");
        appendLog(QString("[Step A/C] Local images missing in %1. Starting smart download & extraction...").arg(resDir), "#F59E0B");
        if (!hasLocalSys) appendLog("  - system.img is missing", "#F59E0B");
        if (!hasLocalVnd) appendLog("  - vendor.img is missing", "#F59E0B");
    }

    // Build initialization command
    QStringList initArgs = {
        "init",
        "-f",
        "-i", resDir,
        "-c", systemImageUrl(),
        "-v", vendorImageUrl()
    };

    QString exeProg = PathResolver::resolveLinstackCommand(initArgs);

    // Determine elevated runner (prefer pkexec, fallback to sudo or direct)
    QString runnerProg = "pkexec";
    QStringList runnerArgs;

    if (QStandardPaths::findExecutable("pkexec").isEmpty()) {
        runnerProg = exeProg;
        runnerArgs = initArgs;
    } else {
        runnerArgs << exeProg;
        runnerArgs << initArgs;
    }

    if (m_process) {
        m_process->deleteLater();
    }
    m_process = new QProcess(this);
    PathResolver::configureProcess(m_process, initArgs);

    connect(m_process, &QProcess::readyReadStandardOutput, this, &InitializationDialog::handleProcessOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &InitializationDialog::handleProcessError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &InitializationDialog::handleProcessFinished);

    appendLog(QString("Executing: %1 %2").arg(runnerProg, runnerArgs.join(" ")), "#9CA3AF");
    m_process->start(runnerProg, runnerArgs);
}

void InitializationDialog::handleProcessOutput()
{
    if (!m_process) return;
    QByteArray rawData = m_process->readAllStandardOutput();
    QString text = QString::fromUtf8(rawData);

    // Parse progress percentage
    static QRegularExpression pctRegex("(\\d{1,3}(?:\\.\\d+)?)%");
    QRegularExpressionMatch match = pctRegex.match(text);
    if (match.hasMatch()) {
        double pct = match.captured(1).toDouble();
        if (pct >= 0.0 && pct <= 100.0) {
            m_progressBar->setValue(static_cast<int>(pct));
        }
    }

    if (text.contains("Downloading system image", Qt::CaseInsensitive)) {
        m_lblStatus->setText("Downloading Android system image archive...");
    } else if (text.contains("Downloading vendor image", Qt::CaseInsensitive)) {
        m_lblStatus->setText("Downloading Android vendor image archive...");
    } else if (text.contains("Extracting", Qt::CaseInsensitive)) {
        m_lblStatus->setText("Extracting Android partition images...");
        m_progressBar->setValue(qMax(m_progressBar->value(), 85));
    } else if (text.contains("Configured LinStack", Qt::CaseInsensitive)) {
        m_lblStatus->setText("Configuring container runtime & binder...");
        m_progressBar->setValue(95);
    }

    // Process output lines
    QStringList lines = text.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (!line.trimmed().isEmpty()) {
            appendLog(line.trimmed());
        }
    }
}

void InitializationDialog::handleProcessError()
{
    if (!m_process) return;
    QByteArray rawData = m_process->readAllStandardError();
    QString text = QString::fromUtf8(rawData);
    if (text.contains("No session for cookie") || text.contains("Cannot register authentication agent") || text.contains("Not authorized")) {
        appendLog("[Polkit] Authentication failed or no Polkit agent available in current session. Run 'sudo linstack init' in a terminal as fallback.", "#F59E0B");
    }
    QStringList lines = text.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (!line.trimmed().isEmpty()) {
            appendLog(line.trimmed(), "#EF4444");
        }
    }
}

void InitializationDialog::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_isRunning = false;
    m_btnInit->setEnabled(true);
    m_btnCancel->setEnabled(true);
    m_editSystemUrl->setEnabled(true);
    m_editVendorUrl->setEnabled(true);

    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        m_progressBar->setValue(100);
        m_lblStatus->setText("Container initialized successfully!");
        appendLog("LinStack container initialization completed successfully.", "#00FF66");

        // Enable and start linstack-container.service immediately after first-run init
        appendLog("Activating container daemon: systemctl enable --now linstack-container.service...", "#34D399");
        if (!QStandardPaths::findExecutable("pkexec").isEmpty()) {
            QProcess::execute("pkexec", QStringList() << "systemctl" << "enable" << "--now" << "linstack-container.service");
        } else {
            QProcess::execute("systemctl", QStringList() << "enable" << "--now" << "linstack-container.service");
        }

        emit initializationSucceeded();

        QTimer::singleShot(800, this, &QDialog::accept);
    } else {
        m_lblStatus->setText("Initialization failed. Check logs.");
        QString err = QString("Process finished with error code %1.").arg(exitCode);
        appendLog(err, "#EF4444");
        if (exitCode == 126 || exitCode == 127) {
            appendLog("[Hint] Elevated authentication failed or was cancelled. If no graphical Polkit agent is running, run 'sudo linstack init' in a terminal.", "#F59E0B");
        }
        emit initializationFailed(err);
    }
}

void InitializationDialog::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void InitializationDialog::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
}

void InitializationDialog::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Deep Cyber-Emerald backdrop
    p.fillRect(rect(), Theme::ColorBgPanel);

    // Outer neon cyber border
    QPen borderPen(Theme::ColorBorder);
    borderPen.setWidth(1);
    p.setPen(borderPen);
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    // Accent line at top
    p.setPen(Qt::NoPen);
    p.setBrush(Theme::ColorEmerald);
    p.drawRect(0, 0, width(), 2);
}

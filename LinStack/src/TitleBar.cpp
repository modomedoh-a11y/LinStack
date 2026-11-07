#include "TitleBar.h"
#include "Theme.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QAction>
#include <QWindow>

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(48);
    setAttribute(Qt::WA_Hover, true);
    setupUi();
}

void TitleBar::setupUi()
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 0, 0, 0);
    layout->setSpacing(8);

    // --- Left Section: Branding & Linstack Status ---
    // Logo Icon (Custom cyber hexagon badge)
    m_logoIconLabel = new QLabel(this);
    m_logoIconLabel->setFixedSize(28, 28);
    {
        QPixmap logoPix(56, 56);
        logoPix.fill(Qt::transparent);
        QPainter lp(&logoPix);
        lp.setRenderHint(QPainter::Antialiasing, true);
        lp.scale(2, 2);

        // Hexagon background
        QPainterPath hex;
        const qreal cx = 14, cy = 14, r = 13;
        for (int i = 0; i < 6; ++i) {
            qreal angle = (i * 60 - 30) * M_PI / 180.0;
            qreal x = cx + r * std::cos(angle);
            qreal y = cy + r * std::sin(angle);
            if (i == 0) hex.moveTo(x, y);
            else hex.lineTo(x, y);
        }
        hex.closeSubpath();
        lp.fillPath(hex, QColor(16, 185, 129, 40));
        lp.setPen(QPen(Theme::ColorEmerald, 1.8));
        lp.drawPath(hex);

        // Cyber 'S' / Android antenna accent
        QPen pInner(Theme::ColorNeon, 2.0);
        pInner.setCapStyle(Qt::RoundCap);
        lp.setPen(pInner);
        lp.drawLine(QPointF(9, 10), QPointF(19, 10));
        lp.drawLine(QPointF(9, 10), QPointF(9, 14));
        lp.drawLine(QPointF(9, 14), QPointF(19, 14));
        lp.drawLine(QPointF(19, 14), QPointF(19, 18));
        lp.drawLine(QPointF(9, 18), QPointF(19, 18));

        m_logoIconLabel->setPixmap(logoPix);
    }
    layout->addWidget(m_logoIconLabel);

    // Title: LinStack
    m_titleLabel = new QLabel(this);
    m_titleLabel->setText("<span style='font-size:15px; font-weight:800; color:#F9FAFB;'>lin</span><span style='font-size:15px; font-weight:800; color:#00FF66;'>Stack</span>");
    layout->addWidget(m_titleLabel);

    // Version Badge: v1.0
    m_versionBadge = new QLabel("v1.0", this);
    m_versionBadge->setStyleSheet(
        "background-color: rgba(16, 185, 129, 0.15);"
        "color: #10B981;"
        "border: 1px solid rgba(16, 185, 129, 0.4);"
        "border-radius: 4px;"
        "padding: 1px 5px;"
        "font-size: 10px;"
        "font-weight: 700;"
    );
    layout->addWidget(m_versionBadge);

    // Separator line
    auto *sep1 = new QFrame(this);
    sep1->setFrameShape(QFrame::VLine);
    sep1->setStyleSheet("color: #1F2937; margin-top: 14px; margin-bottom: 14px;");
    layout->addWidget(sep1);

    // Engine Status
    m_statusDot = new QLabel(this);
    m_statusDot->setFixedSize(8, 8);
    m_statusDot->setStyleSheet("background-color: #00FF66; border-radius: 4px;");
    layout->addWidget(m_statusDot);

    m_statusText = new QLabel("Linstack Ready", this);
    m_statusText->setStyleSheet("color: #9CA3AF; font-size: 11px; font-weight: 500;");
    layout->addWidget(m_statusText);

    layout->addStretch(1);

    // --- Center Section: Play Store Search Bar ---
    auto *searchContainer = new QWidget(this);
    searchContainer->setFixedWidth(440);
    searchContainer->setFixedHeight(34);
    searchContainer->setStyleSheet(
        "QWidget {"
        "  background-color: #0D121D;"
        "  border: 1px solid #1F2937;"
        "  border-radius: 17px;"
        "}"
        "QWidget:hover {"
        "  border: 1px solid rgba(16, 185, 129, 0.6);"
        "}"
        "QLineEdit:focus {"
        "  border: none;"
        "}"
    );

    auto *searchLayout = new QHBoxLayout(searchContainer);
    searchLayout->setContentsMargins(10, 0, 8, 0);
    searchLayout->setSpacing(6);

    auto *searchIcon = new QLabel(searchContainer);
    searchIcon->setPixmap(Theme::renderIconPixmap(Theme::IconType::Search, Theme::ColorTextMuted, 16));
    searchLayout->addWidget(searchIcon);

    m_searchEdit = new QLineEdit(searchContainer);
    m_searchEdit->setPlaceholderText("Search in Play Store (e.g. Free Fire, PUBG, Roblox...)");
    m_searchEdit->setStyleSheet(
        "QLineEdit {"
        "  background: transparent;"
        "  border: none;"
        "  color: #F9FAFB;"
        "  font-size: 12px;"
        "  padding: 0px;"
        "}"
        "QLineEdit::placeholder {"
        "  color: #6B7280;"
        "}"
    );
    searchLayout->addWidget(m_searchEdit);

    // Clear / Submit button
    m_searchBtn = new QPushButton(searchContainer);
    m_searchBtn->setFixedSize(20, 20);
    m_searchBtn->setCursor(Qt::PointingHandCursor);
    m_searchBtn->setIcon(Theme::createIcon(Theme::IconType::PlayStore, Theme::ColorEmerald, 14));
    m_searchBtn->setToolTip("Search in Play Store");
    m_searchBtn->setStyleSheet("QPushButton { background: transparent; border: none; } QPushButton:hover { background: rgba(16, 185, 129, 0.2); border-radius: 10px; }");
    searchLayout->addWidget(m_searchBtn);

    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]() {
        if (!m_searchEdit->text().trimmed().isEmpty()) {
            emit searchTriggered(m_searchEdit->text().trimmed());
        }
    });
    connect(m_searchBtn, &QPushButton::clicked, this, [this]() {
        if (!m_searchEdit->text().trimmed().isEmpty()) {
            emit searchTriggered(m_searchEdit->text().trimmed());
        }
    });

    layout->addWidget(searchContainer);
    layout->addStretch(1);

    // --- Right Section: FPS, Help & Window Controls ---
    m_fpsBadge = new QLabel("60 FPS", this);
    m_fpsBadge->setStyleSheet(
        "background-color: #0D121D;"
        "color: #00FF66;"
        "border: 1px solid #1F2937;"
        "border-radius: 4px;"
        "padding: 2px 7px;"
        "font-size: 11px;"
        "font-weight: 700;"
        "font-family: monospace;"
    );
    layout->addWidget(m_fpsBadge);

    // Help Button
    m_helpBtn = new QPushButton(this);
    m_helpBtn->setFixedSize(36, 32);
    m_helpBtn->setIcon(Theme::createIcon(Theme::IconType::Help, Theme::ColorTextMuted, 16));
    m_helpBtn->setToolTip("About LinStack 1.0 & Architecture");
    m_helpBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 4px; }"
        "QPushButton:hover { background: #161F30; color: #10B981; }"
    );
    connect(m_helpBtn, &QPushButton::clicked, this, &TitleBar::helpRequested);
    layout->addWidget(m_helpBtn);

    // Minimize Button
    m_minBtn = new QPushButton(this);
    m_minBtn->setFixedSize(40, 48);
    m_minBtn->setIcon(Theme::createIcon(Theme::IconType::WindowMinimize, Theme::ColorTextMuted, 14));
    m_minBtn->setToolTip("Minimize");
    m_minBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; margin: 0; }"
        "QPushButton:hover { background: #161F30; }"
    );
    connect(m_minBtn, &QPushButton::clicked, this, &TitleBar::minimizeRequested);
    layout->addWidget(m_minBtn);

    // Maximize Button
    m_maxBtn = new QPushButton(this);
    m_maxBtn->setFixedSize(40, 48);
    m_maxBtn->setIcon(Theme::createIcon(Theme::IconType::WindowMaximize, Theme::ColorTextMuted, 14));
    m_maxBtn->setToolTip("Maximize");
    m_maxBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; margin: 0; }"
        "QPushButton:hover { background: #161F30; }"
    );
    connect(m_maxBtn, &QPushButton::clicked, this, &TitleBar::maximizeRequested);
    layout->addWidget(m_maxBtn);

    // Close Button
    m_closeBtn = new QPushButton(this);
    m_closeBtn->setFixedSize(44, 48);
    m_closeBtn->setIcon(Theme::createIcon(Theme::IconType::WindowClose, Theme::ColorTextMuted, 14));
    m_closeBtn->setToolTip("Close LinStack");
    m_closeBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; margin: 0; }"
        "QPushButton:hover { background: #EF4444; }"
    );
    connect(m_closeBtn, &QPushButton::clicked, this, &TitleBar::closeRequested);
    layout->addWidget(m_closeBtn);
}

void TitleBar::setMaximizedState(bool maximized)
{
    m_isMaximized = maximized;
    updateButtonIcons();
}

void TitleBar::updateButtonIcons()
{
    if (m_isMaximized) {
        m_maxBtn->setIcon(Theme::createIcon(Theme::IconType::WindowRestore, Theme::ColorTextMuted, 14));
        m_maxBtn->setToolTip("Restore");
    } else {
        m_maxBtn->setIcon(Theme::createIcon(Theme::IconType::WindowMaximize, Theme::ColorTextMuted, 14));
        m_maxBtn->setToolTip("Maximize");
    }
}

void TitleBar::setEngineStatus(const QString &text, bool active)
{
    m_statusText->setText(text);
    if (active) {
        m_statusDot->setStyleSheet("background-color: #00FF66; border-radius: 4px;");
    } else {
        m_statusDot->setStyleSheet("background-color: #F59E0B; border-radius: 4px;");
    }
}

void TitleBar::setFpsText(const QString &fps)
{
    m_fpsBadge->setText(fps);
}

void TitleBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);

    // Cyber Titlebar Background
    p.fillRect(rect(), Theme::ColorBgPanel);

    // Bottom border separator
    p.setPen(Theme::ColorBorder);
    p.drawLine(0, height() - 1, width(), height() - 1);

    // Subtle emerald glow trace on the bottom border
    QLinearGradient glow(0, 0, width(), 0);
    glow.setColorAt(0.0, QColor(16, 185, 129, 0));
    glow.setColorAt(0.2, QColor(16, 185, 129, 80));
    glow.setColorAt(0.5, QColor(0, 255, 102, 140));
    glow.setColorAt(0.8, QColor(16, 185, 129, 80));
    glow.setColorAt(1.0, QColor(16, 185, 129, 0));
    p.setPen(QPen(QBrush(glow), 1.0));
    p.drawLine(0, height() - 1, width(), height() - 1);
}

void TitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QWidget *child = childAt(event->position().toPoint());
        if (child == nullptr || child == this || child == m_titleLabel || child == m_logoIconLabel) {
            m_isDragging = true;
            m_dragStartPos = event->globalPosition().toPoint();
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void TitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
        QPoint globalPos = event->globalPosition().toPoint();
        QPoint delta = globalPos - m_dragStartPos;
        m_dragStartPos = globalPos;
        emit windowDragged(delta);
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void TitleBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
    }
    QWidget::mouseReleaseEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QWidget *child = childAt(event->position().toPoint());
        if (child == nullptr || child == this || child == m_titleLabel || child == m_logoIconLabel) {
            emit maximizeToggled();
            event->accept();
            return;
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

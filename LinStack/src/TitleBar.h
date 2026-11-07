#ifndef TITLEBAR_H
#define TITLEBAR_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QPoint>

class TitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit TitleBar(QWidget *parent = nullptr);

    void setMaximizedState(bool maximized);
    void setEngineStatus(const QString &text, bool active = true);
    void setFpsText(const QString &fps);

signals:
    void minimizeRequested();
    void maximizeRequested();
    void closeRequested();
    void searchTriggered(const QString &query);
    void helpRequested();
    void windowDragged(const QPoint &globalDelta);
    void maximizeToggled();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void setupUi();
    void updateButtonIcons();

    QLabel *m_logoIconLabel = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_versionBadge = nullptr;
    QLabel *m_statusDot = nullptr;
    QLabel *m_statusText = nullptr;

    QLineEdit *m_searchEdit = nullptr;
    QPushButton *m_searchBtn = nullptr;

    QLabel *m_fpsBadge = nullptr;
    QPushButton *m_helpBtn = nullptr;
    QPushButton *m_minBtn = nullptr;
    QPushButton *m_maxBtn = nullptr;
    QPushButton *m_closeBtn = nullptr;

    bool m_isMaximized = false;
    bool m_isDragging = false;
    QPoint m_dragStartPos;
};

#endif // TITLEBAR_H

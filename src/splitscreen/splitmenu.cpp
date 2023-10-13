#include "splitmenu.h"
#include "x11window.h"
#include "waylandwindow.h"
#include "workspace.h"
#include "wayland_server.h"

#include <QLoggingCategory>
#include <QTranslator>
#include <QCoreApplication>
#include <QToolTip>
#include <QEvent>
#include <QPainter>
#include <QtMath>

#define LONG_PRESS_TIME 500
#define HIDE_DELAY_TIME 300

namespace KWin
{

Q_LOGGING_CATEGORY(SPLIT_MENU, "kwin.splitmenu", QtCriticalMsg);

SplitMenu *SplitMenu::instance()
{
    static SplitMenu menu;
    return &menu;
}

SplitMenu::SplitMenu()
{
    if (waylandServer()) {
        setWindowFlags(Qt::FramelessWindowHint | Qt::X11BypassWindowManagerHint);
    } else {
        setWindowFlags(Qt::X11BypassWindowManagerHint);
    }
    setAttribute(Qt::WA_TranslucentBackground);

    layout = new QHBoxLayout();
    layout->setSpacing(5);
    llabel = new QLabel(this);
    clabel = new QLabel(this);
    rlabel = new QLabel(this);

    layout->addWidget(llabel);
    layout->addWidget(clabel);
    layout->addWidget(rlabel);
    llabel->installEventFilter(this);
    clabel->installEventFilter(this);
    rlabel->installEventFilter(this);

    shadow = new QGraphicsDropShadowEffect(this);
    shadow->setOffset(0, 0);
    shadow->setColor(Qt::gray);
    shadow->setBlurRadius(10);
    this->setGraphicsEffect(shadow);

    QString qm = QString(":/splitmenu/translations/splitmenu_%1.qm").arg(QLocale::system().name());
    auto tran = new QTranslator(this);
    if (tran->load(qm)) {
        qApp->installTranslator(tran);
        qWarning() << "install";
    } else {
        qCDebug(SPLIT_MENU) << "load " << qm << "failed";
    }
}

SplitMenu::~SplitMenu()
{
    if (show_timer)
        delete show_timer;
    if (hide_timer)
        delete hide_timer;
    if (shadow)
        delete shadow;
}

void SplitMenu::enterEvent(QEvent *event)
{
    entered = true;
    QWidget::enterEvent(event);
    stopTime();
}

void SplitMenu::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    Hide(true, true);
}

bool SplitMenu::eventFilter(QObject *obj, QEvent *event)
{
    QString str = "light";
    if (m_isDark) {
        str = "dark";
    }

    int sign = upside ? -1 : 1;

    if (obj == llabel) {
        if (event->type() == QEvent::MouseButtonRelease) {
            llabel->setStyleSheet(QString("background-image:url(:/splitmenu/themes/%1/icons/left_split_hover.svg); background-repeat:no-repeat;background-position:center;").arg(str));
            if (m_client) {
                m_client->setQuickTileFromMenu(QuickTileFlag::Left);
            }
            llabel->setStyleSheet(QString("background-image:url(:/splitmenu/themes/%1/icons/left_split_normal.svg); background-repeat:no-repeat;background-position:center;").arg(str));
            Hide(false, true);
        } else if (event->type() == QEvent::Enter) {
            llabel->setStyleSheet(QString("background-image:url(:/splitmenu/themes/%1/icons/left_split_hover.svg); background-repeat:no-repeat;background-position:center;").arg(str));
            QPoint pos = m_pos;
            pos.setX(m_pos.x() - 70);
            pos.setY(m_pos.y() + sign * 50);
            QToolTip::showText(pos, tr("Tile window to left of screen"), this);
        } else if (event->type() == QEvent::Leave) {
            llabel->setStyleSheet(QString("background-image:url(:/splitmenu/themes/%1/icons/left_split_normal.svg); background-repeat:no-repeat;background-position:center;").arg(str));
            QToolTip::hideText();
        }
        return false;
    } else if (obj == clabel) {
        if (event->type() == QEvent::MouseButtonRelease) {
            clabel->setStyleSheet(QString("background-image:url(:/splitmenu/themes/%1/icons/right_split_hover.svg); background-repeat:no-repeat;background-position:center;").arg(str));
            if (m_client) {
                m_client->setQuickTileFromMenu(QuickTileFlag::Right);
            }
            clabel->setStyleSheet(QString("background-image:url(:/splitmenu/themes/%1/icons/right_split_normal.svg); background-repeat:no-repeat;background-position:center;").arg(str));
            Hide(false, true);
        } else if (event->type() == QEvent::Enter) {
            clabel->setStyleSheet(QString("background-image:url(:/splitmenu/themes/%1/icons/right_split_hover.svg); background-repeat:no-repeat;background-position:center;").arg(str));
            QPoint pos = m_pos;
            pos.setX(m_pos.x() - 20);
            pos.setY(m_pos.y() + sign * 50);
            QToolTip::showText(pos, tr("Tile window to right of screen"), this);
        } else if (event->type() == QEvent::Leave) {
            clabel->setStyleSheet(QString("background-image:url(:/splitmenu/themes/%1/icons/right_split_normal.svg); background-repeat:no-repeat;background-position:center;").arg(str));
            QToolTip::hideText();
        }
        return false;
    } else if (obj == rlabel) {
        QString icon = "max";
        if (m_client && m_client->maximizeMode() == MaximizeMode::MaximizeFull) {
            icon = "restore";
        }
        if (event->type() == QEvent::MouseButtonRelease) {
            rlabel->setStyleSheet(QString("background-image:url(:/splitmenu/themes/%1/icons/%2_split_hover.svg); background-repeat:no-repeat;background-position:center;").arg(str).arg(icon));
            if (m_client) {
                m_client->setQuickTileFromMenu(QuickTileFlag::Maximize, false);
            }
            rlabel->setStyleSheet(QString("background-image:url(:/splitmenu/themes/%1/icons/%2_split_normal.svg); background-repeat:no-repeat;background-position:center;").arg(str).arg(icon));
            Hide(false, true);
        } else if (event->type() == QEvent::Enter) {
            rlabel->setStyleSheet(QString("background-image:url(:/splitmenu/themes/%1/icons/%2_split_hover.svg); background-repeat:no-repeat;background-position:center;").arg(str).arg(icon));
            QPoint pos = m_pos;
            pos.setX(m_pos.x() + 30);
            pos.setY(m_pos.y() + sign * 50);
            if (m_client && m_client->maximizeMode() == MaximizeMode::MaximizeFull) {
                QToolTip::showText(pos, tr("Unmaximize"), this);
            } else {
                QToolTip::showText(pos, tr("Maximize"), this);
            }
        } else if (event->type() == QEvent::Leave) {
            rlabel->setStyleSheet(QString("background-image:url(:/splitmenu/themes/%1/icons/%2_split_normal.svg); background-repeat:no-repeat;background-position:center;").arg(str).arg(icon));
            QToolTip::hideText();
        }
        return false;
    }

    return QWidget::eventFilter(obj, event);
}

void SplitMenu::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    QColor col_menu = m_color;
    painter.setBrush(QBrush(col_menu));

    int spot = (width() / 2) + 12;
    double w = M_PI / 25;
    int fx = spot - 20;

    int sign, rounded_rect_y, wave_base, wave_offset;
    if (upside) {
        sign = -1;
        rounded_rect_y = 10;
        wave_base = height() - 20;
        wave_offset = height() - 11;
    } else {
        sign = 1;
        rounded_rect_y = 20;
        wave_base = 20;
        wave_offset = 11;
    }

    QPainterPath wave;
    QPainterPath painterPath;
    painterPath.addRoundedRect(QRect(5, rounded_rect_y, width() - 10, height() - 30), 14, 14);
    painter.drawPath(painterPath);
    wave.moveTo(fx, wave_base);
    for (int x = 0; x <= 50; x += 1) {
        double waveY = static_cast<double>(sign * (9 * sin(w * x + fx)) + wave_offset);
        wave.lineTo(x + fx, waveY);
    }
    wave.lineTo(spot + 30, wave_base);
    painter.setBrush(QBrush(col_menu));
    painter.drawPath(wave);
}

void SplitMenu::Show(const QRect &button_rect, uint32_t client_id)
{
    if (m_isShow)
        return;
    findClient(client_id);
    if (!m_client /* || !m_client->checkClientAllowToTile() */) {
        m_client = nullptr;
        return;
    }
    stopTime();
    if (!show_timer)
        show_timer = new QTimer();
    show_timer->setSingleShot(true);
    checkTheme();
    checkArea(button_rect, QSize(158, 85));
    connect(show_timer, &QTimer::timeout,
        [this] {
            m_isShow = true;
            show();
        }
    );
    show_timer->start(LONG_PRESS_TIME);
}

void SplitMenu::Hide(bool delay, bool internal)
{
    if ((entered && !internal) || m_keepShowing)
        return;
    stopTime();
    if (!delay || !m_isShow) {
        m_isShow = false;
        entered = false;
        m_client = nullptr;
        hide();
        return;
    }
    if (!hide_timer)
        hide_timer = new QTimer();
    hide_timer->setSingleShot(true);
    connect(hide_timer, &QTimer::timeout,
        [this] {
            m_isShow = false;
            entered = false;
            m_client = nullptr;
            hide();
        }
    );
    hide_timer->start(HIDE_DELAY_TIME);
}

void SplitMenu::findClient(uint32_t client_id)
{
    if (waylandServer()) {
        m_client = workspace()->findWaylandWindow(client_id);
        if (m_client)
            return;
    }
    m_client = workspace()->findClient(Predicate::WindowMatch, client_id);
}

void SplitMenu::checkTheme()
{
    // m_isDark = workspace()->isDarkTheme();
    m_isDark = false;
    QString str;
    if (m_isDark) {
        str = "dark";
        m_color = QColor(0, 0, 0);
    } else {
        str = "light";
        m_color = QColor(255, 255, 255);
    }
    llabel->setStyleSheet(QString("background-image:url(:/splitmenu/themes/%1/icons/left_split_normal.svg); background-repeat:no-repeat;background-position:center;").arg(str));
    clabel->setStyleSheet(QString("background-image:url(:/splitmenu/themes/%1/icons/right_split_normal.svg); background-repeat:no-repeat;background-position:center;").arg(str));
    if (m_client) {
        QString icon = "max";
        if (m_client->maximizeMode() == MaximizeMode::MaximizeFull) {
            icon = "restore";
        }
        rlabel->setStyleSheet(QString("background-image:url(:/splitmenu/themes/%1/icons/%2_split_normal.svg); background-repeat:no-repeat;background-position:center;").arg(str).arg(icon));
    }
}

void SplitMenu::checkArea(const QRect &button_rect, const QSize &size)
{
    m_pos = button_rect.bottomLeft();
    QRect area = workspace()->clientArea(MaximizeArea, m_client).toRect();
    QPoint pos(m_pos.x() - 75, m_pos.y());

    upside = false;
    QMargins margin(7, 14, 7, 2);
    if (pos.x() < area.left()) {
        pos.setX(area.left());
    } else if (pos.x() + size.width() > area.right()) {
        pos.setX(area.right() - size.width());
    }
    if (pos.y() < area.top()) {
        pos.setY(area.top());
    } else if (pos.y() + size.height() > area.bottom()) {
        upside = true;
        margin = QMargins(7, 2, 7, 14);
        m_pos = button_rect.topLeft();
        pos.setY(m_pos.y() - size.height());
    }

    layout->setContentsMargins(margin);
    setLayout(layout);
    setGeometry(QRect(pos, size));
}

void SplitMenu::stopTime()
{
    if (show_timer)
        show_timer->stop();
    if (hide_timer)
        hide_timer->stop();
}

}
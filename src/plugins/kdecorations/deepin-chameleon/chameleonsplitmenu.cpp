#include "chameleonsplitmenu.h"
#include <QEventLoop>
#include <QVBoxLayout>
#include <QDebug>
#include <QPainter>
#include <QBitmap>
#include <QPainterPath>
#include <QtMath>
#include <QTimer>
#include "kwinutils.h"
#include <QTranslator>
#include <QX11Info>

#include "workspace.h"

Q_LOGGING_CATEGORY(SPLIT_MENU, "kwin.splitmenu", QtCriticalMsg);

static void setWindowProperty(xcb_window_t WId, xcb_atom_t propAtom, xcb_atom_t typeAtom, int format, const QByteArray &data)
{
    if (!QX11Info::isPlatformX11())
        return;

    xcb_connection_t* conn = QX11Info::connection();

    if (format == 0 && data.isEmpty()) {
        xcb_delete_property(conn, WId, propAtom);
    } else {
        xcb_change_property(conn, XCB_PROP_MODE_REPLACE, WId, propAtom, typeAtom, format, data.length() * 8 / format, data.constData());
    }
}

xcb_atom_t internAtom(const char *name, bool only_if_exists)
{
    if (!name || *name == 0)
        return XCB_NONE;

    if (!QX11Info::isPlatformX11())
        return XCB_NONE;

    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(QX11Info::connection(), only_if_exists, strlen(name), name);
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(QX11Info::connection(), cookie, 0);

    if (!reply)
        return XCB_NONE;

    xcb_atom_t atom = reply->atom;
    free(reply);

    return atom;
}

ChameleonSplitMenu::ChameleonSplitMenu(QWidget *parent, bool isSupportFourSplit) : QWidget (parent)
{
    if (QX11Info::isPlatformX11()) {
        setWindowFlags(Qt::X11BypassWindowManagerHint);
        int force_decorate = 1;
        setWindowProperty(this->winId(), internAtom("_DEEPIN_FORCE_DECORATE", false),
                                 XCB_ATOM_CARDINAL, 32, QByteArray(reinterpret_cast<char*>(&force_decorate), sizeof(force_decorate) / sizeof(char)));
    } else {
        setWindowFlags(Qt::Popup | Qt::X11BypassWindowManagerHint);
    }
    setAttribute(Qt::WA_TranslucentBackground, true);

    QHBoxLayout *layout = new QHBoxLayout();
    blabel = new QLabel(this);

    llabel = new QLabel(this);
    llabel->setStyleSheet("background-image:url(:/deepin/themes/deepin/light/icons/two_split_normal.svg); background-repeat:no-repeat;");

    clabel = new QLabel(this);
    clabel->setStyleSheet("background-image:url(:/deepin/themes/deepin/light/icons/two_split_normal.svg); background-repeat:no-repeat;");

    if (isSupportFourSplit) {
        m_isSupportFourSplit = isSupportFourSplit;
        threeLabelLeft = new QLabel(this);
        threeLabelLeft->setStyleSheet("background-image:url(:/deepin/themes/deepin/light/icons/two_split_normal.svg); background-repeat:no-repeat;");

        threeLabelTopRight = new QLabel(this);
        threeLabelTopRight->setStyleSheet("background-image:url(:/deepin/themes/deepin/light/icons/four_split_normal.svg); background-repeat:no-repeat;");
        threeLabelBottomRight = new QLabel(this);
        threeLabelBottomRight->setStyleSheet("background-image:url(:/deepin/themes/deepin/light/icons/four_split_normal.svg); background-repeat:no-repeat;");

        threeLabelTopLeft = new QLabel(this);
        threeLabelTopLeft->setStyleSheet("background-image:url(:/deepin/themes/deepin/light/icons/four_split_normal.svg); background-repeat:no-repeat;");
        threeLabelBottomLeft = new QLabel(this);
        threeLabelBottomLeft->setStyleSheet("background-image:url(:/deepin/themes/deepin/light/icons/four_split_normal.svg); background-repeat:no-repeat;");
        threeLabelRight = new QLabel(this);
        threeLabelRight->setStyleSheet("background-image:url(:/deepin/themes/deepin/light/icons/two_split_normal.svg); background-repeat:no-repeat;");

        fourLabelTopLeft = new QLabel(this);
        fourLabelTopLeft->setStyleSheet("background-image:url(:/deepin/themes/deepin/light/icons/four_split_normal.svg); background-repeat:no-repeat;");
        fourLabelBottomLeft = new QLabel(this);
        fourLabelBottomLeft->setStyleSheet("background-image:url(:/deepin/themes/deepin/light/icons/four_split_normal.svg); background-repeat:no-repeat;");
        fourLabelTopRight = new QLabel(this);
        fourLabelTopRight->setStyleSheet("background-image:url(:/deepin/themes/deepin/light/icons/four_split_normal.svg); background-repeat:no-repeat;");
        fourLabelBottomRight = new QLabel(this);
        fourLabelBottomRight->setStyleSheet("background-image:url(:/deepin/themes/deepin/light/icons/four_split_normal.svg); background-repeat:no-repeat;");

        layout->addWidget(threeLabelLeft);
        layout->addWidget(threeLabelTopRight);
        layout->addWidget(threeLabelBottomRight);

        layout->addWidget(threeLabelTopLeft);
        layout->addWidget(threeLabelBottomLeft);
        layout->addWidget(threeLabelRight);

        layout->addWidget(fourLabelTopLeft);
        layout->addWidget(fourLabelBottomLeft);
        layout->addWidget(fourLabelTopRight);
        layout->addWidget(fourLabelBottomRight);

        threeLabelLeft->installEventFilter(this);
        threeLabelTopRight->installEventFilter(this);
        threeLabelBottomRight->installEventFilter(this);

        threeLabelTopLeft->installEventFilter(this);
        threeLabelBottomLeft->installEventFilter(this);
        threeLabelRight->installEventFilter(this);

        fourLabelTopLeft->installEventFilter(this);
        fourLabelBottomLeft->installEventFilter(this);
        fourLabelTopRight->installEventFilter(this);
        fourLabelBottomRight->installEventFilter(this);
        blabel->setStyleSheet("background-image:url(:/deepin/themes/deepin/light/icons/four_split_frame.svg); background-repeat:no-repeat;");
    } else {
        blabel->setStyleSheet("background-image:url(:/deepin/themes/deepin/light/icons/two_split_frame.svg); background-repeat:no-repeat;");
    }
    layout->addWidget(blabel);
    layout->addWidget(llabel);
    layout->addWidget(clabel);

    llabel->installEventFilter(this);
    clabel->installEventFilter(this);
    setLayout(layout);

    QString qm = QString(":/splitmenu/translations/splitmenu_%1.qm").arg(QLocale::system().name());
    auto tran = new QTranslator(this);
    if (tran->load(qm)) {
        qApp->installTranslator(tran);
    } else {
        qCDebug(SPLIT_MENU) << "load " << qm << "failed";
    }
}

ChameleonSplitMenu::~ChameleonSplitMenu()
{
    if (tip_timer) {
        delete tip_timer;
        tip_timer = nullptr;
    }
    if (shadow) {
        delete shadow;
        shadow = nullptr;
    }
}

void ChameleonSplitMenu::enterEvent(QEvent *e)
{
    QWidget::enterEvent(e);
    stopTime();
}

void ChameleonSplitMenu::leaveEvent(QEvent *e)
{
    QWidget::leaveEvent(e);
    if (!m_MenuSt)
        startTime();
}

void ChameleonSplitMenu::paintEvent(QPaintEvent *e)
{
    QPainter painter(this);

    QColor col_menu = QColor("#EEEEEE");
    col_menu.setAlpha(255 * 0.8);
    llabel->setGeometry(QRect(14, 14, 44, 58));
    clabel->setGeometry(QRect(60, 14, 44, 58));
    if (m_isSupportFourSplit) {
        threeLabelLeft->setGeometry(QRect(120, 14, 44, 58));
        threeLabelTopRight->setGeometry(QRect(166, 14, 44, 28));
        threeLabelBottomRight->setGeometry(QRect(166, 44, 44, 28));

        threeLabelTopLeft->setGeometry(QRect(14, 88, 44, 28));
        threeLabelBottomLeft->setGeometry(QRect(14, 118, 44, 28));
        threeLabelRight->setGeometry(QRect(60, 88, 44, 58));

        fourLabelTopLeft->setGeometry(QRect(120, 88, 44, 28));
        fourLabelBottomLeft->setGeometry(QRect(120, 118, 44, 28));
        fourLabelTopRight->setGeometry(QRect(166, 88, 44, 28));
        fourLabelBottomRight->setGeometry(QRect(166, 118, 44, 28));

        blabel->setGeometry(QRect(0, 0, 224, 160));
    } else {
        blabel->setGeometry(QRect(0, 0, 119, 87));
    }
    painter.setRenderHints(QPainter::Antialiasing, true);

    painter.setPen(QPen(Qt::transparent, 1));
    painter.setBrush(QBrush(col_menu));
    QPainterPath painterPath;
    painterPath.addRoundedRect(QRect(0, 0, width(), height()), 18, 18);
    painter.drawPath(painterPath);
}

bool ChameleonSplitMenu::eventFilter(QObject *obj, QEvent *event)
{
    QString str = "light";
    /*if (m_isDark) {
        str = "dark";
    }*/
    if (obj == llabel) {
        if (event->type() == QEvent::MouseButtonRelease) {
            llabel->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/two_split_hover.svg); background-repeat:no-repeat;").arg(str));
            if (m_client) {
                KWinUtils::Window::setQuikTileMode(m_client, int(KWin::SplitMode::Two), (int)QuickTileFlag::Left, true);
            }
            llabel->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/two_split_normal.svg); background-repeat:no-repeat;").arg(str));
            Hide();
        } else if (event->type() == QEvent::Enter) {
            llabel->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/two_split_hover.svg); background-repeat:no-repeat;").arg(str));
            QPoint pos = m_pos;
            pos.setX(m_pos.x() - 70);
            pos.setY(m_pos.y() + 50);
        } else if (event->type() == QEvent::Leave) {
            llabel->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/two_split_normal.svg); background-repeat:no-repeat;").arg(str));
        }
        return false;
    } else if (obj == clabel) {
        if (event->type() == QEvent::MouseButtonRelease) {
            clabel->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/two_split_hover.svg); background-repeat:no-repeat;").arg(str));
            if (m_client) {
                KWinUtils::Window::setQuikTileMode(m_client, int(KWin::SplitMode::Two), (int)QuickTileFlag::Right, true);
            }
            clabel->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/two_split_normal.svg); background-repeat:no-repeat;").arg(str));
            Hide();
        } else if (event->type() == QEvent::Enter) {
            clabel->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/two_split_hover.svg); background-repeat:no-repeat;").arg(str));
            QPoint pos = m_pos;
            pos.setX(m_pos.x() - 20);
            pos.setY(m_pos.y() + 50);
        } else if (event->type() == QEvent::Leave) {
            clabel->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/two_split_normal.svg); background-repeat:no-repeat;").arg(str));
        }
        return false;
    }
    if (m_isSupportFourSplit) {
        if (obj == threeLabelLeft) {
            if (event->type() == QEvent::MouseButtonRelease) {
                threeLabelLeft->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/two_split_hover.svg); background-repeat:no-repeat;").arg(str));
                if (m_client) {
                    KWinUtils::Window::setQuikTileMode(m_client, int(KWin::SplitMode::Three), (int)QuickTileFlag::Left, true);
                }
                threeLabelLeft->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/two_split_normal.svg); background-repeat:no-repeat;").arg(str));
                Hide();
            } else if (event->type() == QEvent::Enter) {
                threeLabelLeft->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/two_split_hover.svg); background-repeat:no-repeat;").arg(str));
                QPoint pos = m_pos;
                pos.setX(m_pos.x() - 20);
                pos.setY(m_pos.y() + 50);
            } else if (event->type() == QEvent::Leave) {
                threeLabelLeft->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/two_split_normal.svg); background-repeat:no-repeat;").arg(str));
            }
            return false;
        } else if (obj == threeLabelTopRight) {
            if (event->type() == QEvent::MouseButtonRelease) {
                threeLabelTopRight->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_hover.svg); background-repeat:no-repeat;").arg(str));
                if (m_client) {
                    KWinUtils::Window::setQuikTileMode(m_client, int(KWin::SplitMode::Three), (int)QuickTileFlag::Top | (int)QuickTileFlag::Right, true);
                }
                threeLabelTopRight->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_normal.svg); background-repeat:no-repeat;").arg(str));
                Hide();
            } else if (event->type() == QEvent::Enter) {
                threeLabelTopRight->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_hover.svg); background-repeat:no-repeat;").arg(str));
                QPoint pos = m_pos;
                pos.setX(m_pos.x() - 20);
                pos.setY(m_pos.y() + 50);
            } else if (event->type() == QEvent::Leave) {
                threeLabelTopRight->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_normal.svg); background-repeat:no-repeat;").arg(str));
            }
            return false;

        } else if (obj == threeLabelBottomRight) {
            if (event->type() == QEvent::MouseButtonRelease) {
                threeLabelBottomRight->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_hover.svg); background-repeat:no-repeat;").arg(str));
                if (m_client) {
                    KWinUtils::Window::setQuikTileMode(m_client, int(KWin::SplitMode::Three), (int)QuickTileFlag::Bottom | (int)QuickTileFlag::Right, true);
                }
                threeLabelBottomRight->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_normal.svg); background-repeat:no-repeat;").arg(str));
                Hide();
            } else if (event->type() == QEvent::Enter) {
                threeLabelBottomRight->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_hover.svg); background-repeat:no-repeat;").arg(str));
                QPoint pos = m_pos;
                pos.setX(m_pos.x() - 20);
                pos.setY(m_pos.y() + 50);
        } else if (event->type() == QEvent::Leave) {
            threeLabelBottomRight->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_normal.svg); background-repeat:no-repeat;").arg(str));
        }
        return false;

        } else if (obj == threeLabelTopLeft) {
            if (event->type() == QEvent::MouseButtonRelease) {
                threeLabelTopLeft->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_hover.svg); background-repeat:no-repeat;").arg(str));
                if (m_client) {
                    KWinUtils::Window::setQuikTileMode(m_client, int(KWin::SplitMode::Three), (int)QuickTileFlag::Top | (int)QuickTileFlag::Left, true);
                }
                threeLabelTopLeft->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_normal.svg); background-repeat:no-repeat;").arg(str));
                Hide();
            } else if (event->type() == QEvent::Enter) {
                threeLabelTopLeft->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_hover.svg); background-repeat:no-repeat;").arg(str));
                QPoint pos = m_pos;
                pos.setX(m_pos.x() - 20);
                pos.setY(m_pos.y() + 50);
            } else if (event->type() == QEvent::Leave) {
                threeLabelTopLeft->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_normal.svg); background-repeat:no-repeat;").arg(str));
            }
            return false;

        } else if (obj == threeLabelBottomLeft) {
            if (event->type() == QEvent::MouseButtonRelease) {
                threeLabelBottomLeft->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_hover.svg); background-repeat:no-repeat;").arg(str));
                if (m_client) {
                    KWinUtils::Window::setQuikTileMode(m_client, int(KWin::SplitMode::Three), (int)QuickTileFlag::Bottom | (int)QuickTileFlag::Left, true);
                }
                threeLabelBottomLeft->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_normal.svg);").arg(str));
                Hide();
            } else if (event->type() == QEvent::Enter) {
                threeLabelBottomLeft->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_hover.svg); background-repeat:no-repeat;").arg(str));
                QPoint pos = m_pos;
                pos.setX(m_pos.x() - 20);
                pos.setY(m_pos.y() + 50);
            } else if (event->type() == QEvent::Leave) {
                threeLabelBottomLeft->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_normal.svg); background-repeat:no-repeat;").arg(str));
            }
            return false;

        } else if (obj == threeLabelRight) {
            if (event->type() == QEvent::MouseButtonRelease) {
                threeLabelRight->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/two_split_hover.svg); background-repeat:no-repeat;").arg(str));
                if (m_client) {
                    KWinUtils::Window::setQuikTileMode(m_client, int(KWin::SplitMode::Three), (int)(QuickTileFlag::Right), true);
                }
                threeLabelRight->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/two_split_normal.svg); background-repeat:no-repeat;").arg(str));
                Hide();
            } else if (event->type() == QEvent::Enter) {
                threeLabelRight->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/two_split_hover.svg); background-repeat:no-repeat;").arg(str));
                QPoint pos = m_pos;
                pos.setX(m_pos.x() - 20);
                pos.setY(m_pos.y() + 50);
            } else if (event->type() == QEvent::Leave) {
                threeLabelRight->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/two_split_normal.svg); background-repeat:no-repeat;").arg(str));
            }
            return false;

        } else if (obj == fourLabelTopLeft) {
            if (event->type() == QEvent::MouseButtonRelease) {
                fourLabelTopLeft->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_hover.svg); background-repeat:no-repeat;").arg(str));
                if (m_client) {
                    KWinUtils::Window::setQuikTileMode(m_client, int(KWin::SplitMode::Four), (int)QuickTileFlag::Top | (int)QuickTileFlag::Left, true);
                }
                fourLabelTopLeft->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_normal.svg); background-repeat:no-repeat;").arg(str));
                Hide();
            } else if (event->type() == QEvent::Enter) {
                fourLabelTopLeft->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_hover.svg); background-repeat:no-repeat;").arg(str));
                QPoint pos = m_pos;
                pos.setX(m_pos.x() - 20);
                pos.setY(m_pos.y() + 50);
            } else if (event->type() == QEvent::Leave) {
                fourLabelTopLeft->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_normal.svg); background-repeat:no-repeat;").arg(str));
            }
            return false;

        } else if (obj == fourLabelBottomLeft) {
            if (event->type() == QEvent::MouseButtonRelease) {
                fourLabelBottomLeft->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_hover.svg); background-repeat:no-repeat;").arg(str));
                if (m_client) {
                    KWinUtils::Window::setQuikTileMode(m_client, int(KWin::SplitMode::Four), (int)QuickTileFlag::Bottom | (int)QuickTileFlag::Left, true);
                }
                fourLabelBottomLeft->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_normal.svg); background-repeat:no-repeat;").arg(str));
                Hide();
            } else if (event->type() == QEvent::Enter) {
                fourLabelBottomLeft->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_hover.svg); background-repeat:no-repeat;").arg(str));
                QPoint pos = m_pos;
                pos.setX(m_pos.x() - 20);
                pos.setY(m_pos.y() + 50);
            } else if (event->type() == QEvent::Leave) {
                fourLabelBottomLeft->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_normal.svg); background-repeat:no-repeat;").arg(str));
            }
            return false;

        } else if (obj == fourLabelTopRight) {
            if (event->type() == QEvent::MouseButtonRelease) {
                fourLabelTopRight->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_hover.svg); background-repeat:no-repeat;").arg(str));
                if (m_client) {
                    KWinUtils::Window::setQuikTileMode(m_client, int(KWin::SplitMode::Four), (int)QuickTileFlag::Top | (int)QuickTileFlag::Right, true);
                }
                fourLabelTopRight->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_normal.svg); background-repeat:no-repeat;").arg(str));
                Hide();
            } else if (event->type() == QEvent::Enter) {
                fourLabelTopRight->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_hover.svg); background-repeat:no-repeat;").arg(str));
                QPoint pos = m_pos;
                pos.setX(m_pos.x() - 20);
                pos.setY(m_pos.y() + 50);
            } else if (event->type() == QEvent::Leave) {
                fourLabelTopRight->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_normal.svg); background-repeat:no-repeat;").arg(str));
            }
            return false;

        } else if (obj == fourLabelBottomRight) {
            if (event->type() == QEvent::MouseButtonRelease) {
                fourLabelBottomRight->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_hover.svg); background-repeat:no-repeat;").arg(str));
                if (m_client) {
                    KWinUtils::Window::setQuikTileMode(m_client, int(KWin::SplitMode::Four), (int)QuickTileFlag::Bottom | (int)QuickTileFlag::Right, true);
                }
                fourLabelBottomRight->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_normal.svg); background-repeat:no-repeat;").arg(str));
                Hide();
            } else if (event->type() == QEvent::Enter) {
                fourLabelBottomRight->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_hover.svg); background-repeat:no-repeat;").arg(str));
                QPoint pos = m_pos;
                pos.setX(m_pos.x() - 20);
                pos.setY(m_pos.y() + 50);
            } else if (event->type() == QEvent::Leave) {
                fourLabelBottomRight->setStyleSheet(QString("background-image:url(:/deepin/themes/deepin/%1/icons/four_split_normal.svg); background-repeat:no-repeat;").arg(str));
            }
            return false;
        }
    }

    return QWidget::eventFilter(obj, event);
}

void ChameleonSplitMenu::CheckTheme()
{
    //Dark themes are not supported
    // QColor clr(255, 255, 255, 255);
    // if (m_color == clr) {
        m_isDark = false;
    // } else {
    //     m_isDark = true;
    // }
}

void ChameleonSplitMenu::Show(const QRect& screenGeom, QPoint pos, QColor color)
{
    if (m_isShow)
        return;
    m_isShow = true;
    m_pos = pos;
    m_color = color;
    CheckTheme();

    QSize menuSize;
    if (m_isSupportFourSplit) {//surpport four split
        pos.setX(m_pos.x() - 91);
        menuSize = QSize(224, 160);
    } else {
        pos.setX(m_pos.x() - 37);
        menuSize = QSize(119, 87);
    }
    setFixedSize(menuSize);
    QRect displayRect = QRect(pos, QSize(menuSize.width(), menuSize.height()));
    //change rect to show menu
    if (!screenGeom.contains(displayRect.bottomRight())) {
        int x = displayRect.right() - screenGeom.right();
        int y = displayRect.bottom() - screenGeom.bottom();
        if (x > 0)
            displayRect.setX(displayRect.x() - x);
        if (y > 0)
            displayRect.setY(displayRect.y() - y);
    }
    if (!screenGeom.contains(displayRect.topLeft())) {
        int x = screenGeom.x() - displayRect.x();
        int y = screenGeom.y() - displayRect.y();
        if (x > 0)
            displayRect.setX(displayRect.x() + x);
        if (y > 0)
            displayRect.setY(displayRect.y() + y);
    }

    QRect rect = QRect(displayRect.x(), displayRect.y(), menuSize.width(), menuSize.height());
    setGeometry(rect);
    show();
    update();
}

void ChameleonSplitMenu::Hide()
{
    m_isShow = false;
    hide();
}

bool ChameleonSplitMenu::isShow()
{
    return m_isShow;
}

void ChameleonSplitMenu::setShowSt(bool flag)
{
    m_MenuSt = flag;
}

void ChameleonSplitMenu::setEffect(QObject *client)
{
    m_client = client;
}

void ChameleonSplitMenu::setEffect(WId id)
{
    m_client = KWinUtils::findClient(KWinUtils::Predicate::WindowMatch, id);
}

void ChameleonSplitMenu::setSupportFourSplit(bool flag)
{
    m_isSupportFourSplit = flag;
}

void ChameleonSplitMenu::startTime(int tm)
{
    if (!tip_timer) {
        tip_timer = new QTimer();
        tip_timer->setSingleShot(true);
        connect(tip_timer, &QTimer::timeout, [this] {
            Hide();
        });
        tip_timer->start(tm);
    } else {
        tip_timer->start(tm);
    }
}

void ChameleonSplitMenu::stopTime()
{
    if (tip_timer) {

        tip_timer->stop();
    }
}

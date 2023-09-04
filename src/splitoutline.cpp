// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "splitoutline.h"
#include "workspace.h"
#include "deepin_kwineffects.h"
#include "splithandler/splithandler.h"
#include "wayland_server.h"
#include "platform.h"
#include "splitmanage.h"
#include "abstract_output.h"

#define CURSOR_LEFT     0
#define CURSOR_RIGHT    1
#define CURSOR_UP       2
#define CURSOR_DOWN     3
#define CURSOR_L_R      4
#define SMALL_LINE_WIDTH 4
#define SMALL_LINE_HEIGHT 50
namespace KWin
{
SplitOutline *SplitOutline::m_instance = nullptr;
SplitOutline *SplitOutline::instance()
{
    if (m_instance == nullptr) {
        m_instance = new SplitOutline();
    }
    return m_instance;
}

 SplitOutline::SplitOutline()
        : QWidget()
{
    setWindowFlags(Qt::X11BypassWindowManagerHint);
    if (waylandServer()) {
        setWindowFlags(Qt::FramelessWindowHint);
    }
    setWindowOpacity(0);
    setCustomCursor(CURSOR_L_R);
    updateWorkspaceArea();
    setAccessibleName("splitoutline");
}

SplitOutline::~SplitOutline()
{
}

void SplitOutline::setCustomCursor(int direct)
{
    m_cursor = direct;
    QCursor cursor;
    if (direct == CURSOR_LEFT) {
        QPixmap pixmap(":/resources/themes/left-arrow.svg");
        cursor = QCursor(pixmap, -1, -1);
    } else if (direct == CURSOR_RIGHT) {
        QPixmap pixmap(":/resources/themes/right-arrow.svg");
        cursor = QCursor(pixmap, -1, -1);
    } else if (direct == CURSOR_DOWN) {
        QPixmap pixmap(":/resources/themes/down-arrow.svg");
        cursor = QCursor(pixmap, -1, -1);
    } else if (direct == CURSOR_UP) {
        QPixmap pixmap(":/resources/themes/up-arrow.svg");
        cursor = QCursor(pixmap, -1, -1);
    } else {
        if (width() > height()) {
            cursor = Qt::SizeVerCursor;
        }
        else {
            cursor = Qt::SizeHorCursor;
        }
    }
    setCursor(cursor);
    if (waylandServer()) {
        m_splitOutlineCursorStatus = direct;
        update();
    }
}

int SplitOutline::getCustomCursor()
{
    return m_cursor;
}

void SplitOutline::mousePressEvent(QMouseEvent* e)
{
    updateWorkspaceArea();
    setWindowOpacity(1);
    m_mainWindowPress = true;

    // handleSplitScreenLayer();
    m_activeClient = Workspace::self()->activeClient();
    if (!m_activeClient)
        return;
    m_activeClientMode = m_activeClient->quickTileMode();
    m_desktop = effects->currentDesktop();//m_activeClient->desktop();
    //m_screen = m_activeClient->output()->name();
    m_screen = effects->screenAt(geometry().center())->name();
    m_workspaceRect = workspace()->clientArea(MaximizeArea, geometry().center(), 1);

    if (width() > height())
        m_isTopDownMove = true;
    if (m_isTopDownMove)
        m_pos = e->screenPos().y();
    else
        m_pos = e->screenPos().x();
    m_showPos = m_pos;
    Q_EMIT effectsEx->signalSplitScreenStartShowMasking(m_screen, m_isTopDownMove);
}

void SplitOutline::mouseMoveEvent(QMouseEvent*e)
{
    if (m_isTopDownMove) {
        m_pos = e->screenPos().y();
    } else {
        m_pos = e->screenPos().x();
    }

    Q_EMIT effectsEx->signalSplitScreenResizeMasking(m_pos);
}

void SplitOutline::mouseReleaseEvent(QMouseEvent* e)
{
    if (m_isTopDownMove) {
        this->move(x(), m_showPos - 7);
        m_Rect = QRect(x(), m_showPos - 7, m_Rect.width(), m_Rect.height());
    } else {
        this->move(m_showPos - 7, y());
        m_Rect = QRect(m_showPos - 7, y(), m_Rect.width(), m_Rect.height());
    }

    SplitManage::instance()->updateSplitRect(m_desktop, m_screen, m_showPos, m_isTopDownMove);
    m_mainWindowPress = false;
    m_pos = 0;
    m_showPos = 0;
    m_isTopDownMove = false;
    Q_EMIT effectsEx->signalSplitScreenExitMasking();
}

void SplitOutline::enterEvent(QEvent *)
{
    setGeometry(m_Rect);
    move(m_Rect.x(), m_Rect.y());
    if (waylandServer()) {
        //kwinApp()->platform()->hideCursor();
        m_splitOutlineCursorStatus = CURSOR_L_R;
    }
    update();
    m_screen = effects->screenAt(geometry().center())->name();
    if (SplitManage::instance()->isShowSplitLine(m_screen))
        setWindowOpacity(1);
}

void SplitOutline::leaveEvent(QEvent *)
{
    if (waylandServer()) {
        update();
        //kwinApp()->platform()->showCursor();
    }
    if (SplitManage::instance()->isShowSplitLine(m_screen))
        setWindowOpacity(0);
}

void SplitOutline::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    if (geometry() != m_Rect) {
        setGeometry(m_Rect);
        move(m_Rect.x(), m_Rect.y());
        update();
    }

    QPainter splitOutlinePainter(this);
    QPen pen;
    pen.setColor(QColor("#BBBBBB"));
    pen.setWidth(1);
    splitOutlinePainter.setRenderHint(QPainter::Antialiasing, true);
    splitOutlinePainter.setBrush(QColor("#DFDFDF"));
    splitOutlinePainter.setPen(pen);
    splitOutlinePainter.drawRect(0, 0, width(), height());

    QPainter smallLine(this);
    smallLine.setRenderHint(QPainter::Antialiasing, true);
    smallLine.setBrush(QColor("#7A7A7A"));
    QPen smallLinePen;
    smallLinePen.setColor(QColor("#7A7A7A"));
    smallLinePen.setWidth(1);
    smallLine.setPen(smallLinePen);

    int smallLineWidth = SMALL_LINE_WIDTH;
    int smallLineHeight = SMALL_LINE_HEIGHT;
    if (width() > height()) {
        smallLineWidth = SMALL_LINE_HEIGHT;
        smallLineHeight = SMALL_LINE_WIDTH;
    }
    int x = (width() - smallLineWidth) / 2;
    int y = (height() - smallLineHeight) / 2;
    smallLine.drawRoundRect(QRect(x, y, smallLineWidth, smallLineHeight), 20, 20);

    if (waylandServer()) {
        QPainter CursorPainter(this);
        if (m_splitOutlineCursorStatus == CURSOR_LEFT) {
            QImage Image(":/resources/themes/left-arrow.svg");
            QImage nImage = Image.scaled(width(),32);
            input()->setCursorShape(Qt::BlankCursor);
            CursorPainter.drawImage(QPoint(-5, QCursor::pos().y()), nImage);
        } else if(m_splitOutlineCursorStatus == CURSOR_RIGHT ){
            QImage Image(":/resources/themes/right-arrow.svg");
            QImage nImage = Image.scaled(width(),32);
            input()->setCursorShape(Qt::BlankCursor);
            CursorPainter.drawImage(QPoint(-5, QCursor::pos().y()), nImage);
        } else if(m_splitOutlineCursorStatus == CURSOR_L_R) {
            QImage Image(":/resources/themes/leftright-arrow.svg");
            QImage nImage = Image.scaled(width(),32);
            if (width() > height()) {
                input()->setCursorShape(Qt::SizeVerCursor);
            } else {
                input()->setCursorShape(Qt::SizeHorCursor);
            }
        }
    }
}

void SplitOutline::updateSplitOutlinePosition()
{
    // {
    //     setGeometry((m_workspaceRect.x() + m_workspaceRect.width()/2)-10, m_workspaceRect.y(), 20, m_workspaceRect.height());
    //     move(m_leftSplitClient->x() + m_leftSplitClient->width() - 10, m_workspaceRect.y());
    //     update();
    // }
}

void SplitOutline::resumeClientLocation()
{
    // if ((m_rightSplitClientRect.isEmpty() || m_leftSplitClientRect.isEmpty()) && !m_workspaceRect.isEmpty()) {
    //     m_leftSplitClientRect = QRect(m_workspaceRect.x(), m_workspaceRect.y(), m_workspaceRect.width()/2, m_workspaceRect.height());
    //     m_rightSplitClientRect = QRect(m_workspaceRect.x() + m_workspaceRect.width()/2, 0, m_workspaceRect.width()/2, m_workspaceRect.height());
    // }

    // m_leftSplitClient->moveResize(QRect(m_leftSplitClientRect.x(), m_workspaceRect.y(), m_leftSplitClientRect.width(), m_workspaceRect.height()));
    // m_leftSplitClient->palette();
    // m_rightSplitClient->moveResize(QRect(m_rightSplitClientRect.x(), m_workspaceRect.y(), m_rightSplitClientRect.width(), m_workspaceRect.height()));
    // m_rightSplitClient->palette();
    // updateSplitOutlinePosition();
}

void SplitOutline::updateWorkspaceArea()
{
    //m_workspaceRect = workspace()->clientArea(MaximizeArea, m_screen, m_desktop);
}

void SplitOutline::handleDockChangePosition()
{
    // updateWorkspaceArea();
    // //if (clientsStatus() == 1)
    // {
    //     m_leftSplitClientRect = m_leftSplitClient->moveResizeGeometry();
    //     m_rightSplitClientRect = m_rightSplitClient->moveResizeGeometry();
    // }
    // updateOutlineStatus();
}

void SplitOutline::handleSplitScreenLayer()
{
    // if (m_leftSplitClient != nullptr && m_rightSplitClient != nullptr) {
    //     if (m_leftSplitClient->isActive()) {
    //         workspace()->raiseClient(m_rightSplitClient);
    //         workspace()->raiseClient(m_leftSplitClient);
    //     } else if (m_rightSplitClient->isActive()) {
    //         workspace()->raiseClient(m_leftSplitClient);
    //         workspace()->raiseClient(m_rightSplitClient);
    //     } else {
    //         workspace()->raiseClient(m_rightSplitClient);
    //         workspace()->raiseClient(m_leftSplitClient);
    //     }
    // }
}

void SplitOutline::updateOutlineStatus()
{
    // if (m_cursor != CURSOR_L_R) {
    //     if (m_leftSplitClientRect.width() > m_rightSplitClientRect.width())
    //         setCustomCursor(CURSOR_LEFT);
    //     else if (m_leftSplitClientRect.width() < m_rightSplitClientRect.width())
    //         setCustomCursor(CURSOR_RIGHT);
    //     else
    //         setCustomCursor(CURSOR_L_R);
    // }

    // updateSplitOutlinePosition();
}

void SplitOutline::setShowPos(int pos, bool isLeftRight)
{
    if (isLeftRight) {
        m_showPos = pos;
        m_cursor = CURSOR_L_R;
    } else {
        if (m_isTopDownMove) {
            if (m_showPos < m_workspaceRect.height() / 2) {
                m_cursor = CURSOR_DOWN;
            } else {
                m_cursor = CURSOR_UP;
            }
        } else {
            if (m_showPos < m_workspaceRect.width() / 2) {
                m_cursor = CURSOR_RIGHT;
            } else {
                m_cursor = CURSOR_LEFT;
            }
        }
    }
    setCustomCursor(m_cursor);
}

void SplitOutline::showOutline(QRect rect)
{
    if (m_mainWindowPress)
        return;
    m_Rect = rect;
    setGeometry(rect);
    move(rect.x(), rect.y());
    m_cursor = CURSOR_L_R;
    setCustomCursor(CURSOR_L_R);
    setWindowOpacity(1);
    update();
    show();
}

void SplitOutline::hideOutline()
{
    hide();
}

}

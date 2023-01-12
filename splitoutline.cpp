// Copyright (C) 2017 ~ 2019 Deepin Technology Co., Ltd.
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "splitoutline.h"
#include "workspace.h"
#include "kwineffects.h"
#include "splithandler/splithandler.h"
#include "wayland_server.h"
#include "platform.h"

#define CURSOR_LEFT     0
#define CURSOR_RIGHT    1
#define CURSOR_L_R      2

#define MAXWIDTH 20
#define MAXPOS 10
#define MINWIDTH 4
#define MINPOS 2

namespace KWin
{
 SplitOutline::SplitOutline(int screen, int desktop)
        : QWidget()
        , m_screen(screen)
        , m_desktop(desktop)
    {
        setWindowFlags(Qt::X11BypassWindowManagerHint);
        if (waylandServer()) {
            setWindowFlags(Qt::FramelessWindowHint);
        }
        setWindowOpacity(0);
        setCustomCursor(CURSOR_L_R);
        updateWorkspaceArea();
        setAccessibleName("splitoutline");
        setWindowTitle("deepin-splitoutline");
        splitoutlineWidth = MINWIDTH;
        splitoutlinePos = MINPOS;
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
        } else {
            cursor = Qt::SizeHorCursor;
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
        setWindowOpacity(1);
        m_mainWindowPress = true;
        maxLeftSplitClientWidth = m_leftSplitClient->maxSize().width();
        minLeftSplitClientWidth = m_leftSplitClient->minSize().width();
        maxRightSplitClientWidth = m_rightSplitClient->maxSize().width();
        minRightSplitClientWidth = m_rightSplitClient->minSize().width();
        m_pos = e->screenPos().x();
        handleSplitScreenLayer();
    }

    void SplitOutline::mouseMoveEvent(QMouseEvent*e)
    {
        if (e->screenPos().x() > (m_pos + 15)) {
            m_pos += 15;
        } else if (e->screenPos().x() < (m_pos - 15)) {
            m_pos -= 15;
        } else {
            m_pos = e->screenPos().x();
        }

        if (clientsStatus() == 1 && m_mainWindowPress == true) {
            const int leftSplitClientWidth = m_pos - m_workspaceRect.x();
            const int rightSplitClientWidth = m_workspaceRect.width() - leftSplitClientWidth;

            if ((minLeftSplitClientWidth < leftSplitClientWidth && leftSplitClientWidth < maxLeftSplitClientWidth)
                && (maxRightSplitClientWidth > rightSplitClientWidth && rightSplitClientWidth > minRightSplitClientWidth)) {

                m_rightSplitClient->setGeometry(m_pos, m_workspaceRect.y(), rightSplitClientWidth, m_workspaceRect.height());
                m_rightSplitClient->palette();

                m_leftSplitClient->setGeometry(m_leftSplitClient->geometry().x(), m_leftSplitClient->geometry().y(), leftSplitClientWidth, m_workspaceRect.height());
                m_leftSplitClient->palette();

                m_rightSplitClientRect = QRect(m_pos, m_workspaceRect.y(), rightSplitClientWidth, m_workspaceRect.height());
                m_leftSplitClientRect = QRect(m_workspaceRect.x(), m_workspaceRect.y(), leftSplitClientWidth, m_workspaceRect.height());
                setCustomCursor(CURSOR_L_R);
                this->move(m_pos - 10, m_workspaceRect.y());
            } else {
                m_pos = m_leftSplitClientRect.width() + m_workspaceRect.x();
                if (leftSplitClientWidth > rightSplitClientWidth) {
                    setCustomCursor(CURSOR_LEFT);
                }
                else {
                    setCustomCursor(CURSOR_RIGHT);
                }
            }
        }
    }

    void SplitOutline::mouseReleaseEvent(QMouseEvent* e)
    {
        m_mainWindowPress = false;
    }

    void SplitOutline::enterEvent(QEvent *)
    {
        splitoutlineWidth = MAXWIDTH;
        splitoutlinePos = MAXPOS;
        if (waylandServer()) {
            m_mouseLeave = false;
            kwinApp()->platform()->hideCursor();
            update();
        }
        updateSplitOutlinePosition();
        setWindowOpacity(1);
    }

    void SplitOutline::leaveEvent(QEvent *)
    {
        splitoutlineWidth = MINWIDTH;
        splitoutlinePos = MINPOS;
        if (waylandServer()) {
            m_mouseLeave = true;
            update();
            kwinApp()->platform()->showCursor();
        }
        updateSplitOutlinePosition();
        setWindowOpacity(0);
    }

    void SplitOutline::paintEvent(QPaintEvent *event)
    {
        Q_UNUSED(event);

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
        smallLine.setPen(Qt::NoPen);
        smallLine.setBrush(QColor("#7A7A7A"));
        smallLine.drawRoundRect(QRect(7, m_workspaceRect.height()/2-25, 6, 50), 30, 30);

        if (waylandServer()) {
            QPainter CursorPainter(this);
            if (m_splitOutlineCursorStatus == CURSOR_LEFT) {
                QImage Image(":/resources/themes/left-arrow.svg");
                QImage nImage = Image.scaled(32, 32, Qt::IgnoreAspectRatio);
                input()->setCursorShape(Qt::BlankCursor);
                CursorPainter.drawImage(QPoint(-5, QCursor::pos().y()), nImage);
            } else if(m_splitOutlineCursorStatus == CURSOR_RIGHT ){
                QImage Image(":/resources/themes/right-arrow.svg");
                QImage nImage = Image.scaled(32, 32, Qt::IgnoreAspectRatio);
                input()->setCursorShape(Qt::BlankCursor);
                CursorPainter.drawImage(QPoint(-5, QCursor::pos().y()), nImage);
            } else if(m_splitOutlineCursorStatus == CURSOR_L_R) {
                QImage Image(":/resources/themes/leftright-arrow.svg");
                QImage nImage = Image.scaled(32, 32, Qt::IgnoreAspectRatio);
                input()->setCursorShape(Qt::BlankCursor);
                CursorPainter.drawImage(QPoint(-5, QCursor::pos().y()), nImage);
            }
            if (m_mouseLeave) {
                input()->setCursorShape(Qt::ArrowCursor);
            }
        }
    }

    void SplitOutline::setLeftSplitClient(AbstractClient* client)
    {
        m_leftSplitClient = client;
        if (clientsStatus() == 1 && !isVisible())
        {
            if(waylandServer()) {
                setGeometry((m_workspaceRect.x() + m_workspaceRect.width()/2) - splitoutlinePos, m_workspaceRect.y(), splitoutlineWidth, m_workspaceRect.height());
            }
            show();
        }
        if (client == nullptr && isVisible()) {
            hide();
            m_leftSplitClientRect = QRect(0, 0, -1, -1);
        }
    }

    void SplitOutline::setRightSplitClient(AbstractClient* client)
    {
        m_rightSplitClient = client;
        if (clientsStatus() == 1 && !isVisible())
        {
            if(waylandServer()) {
                setGeometry((m_workspaceRect.x() + m_workspaceRect.width()/2) - splitoutlinePos, m_workspaceRect.y(), splitoutlineWidth, m_workspaceRect.height());
            }
            show();
        }
        if (client == nullptr && isVisible()) {
            hide();
            m_rightSplitClientRect = QRect(0, 0, -1, -1);
        }
    }

    void SplitOutline::setSplitClient(AbstractClient* client, QuickTileFlag flag)
    {
        if (flag == QuickTileFlag::Left) {
            setLeftSplitClient(client);
        } else if (flag == QuickTileFlag::Right) {
            setRightSplitClient(client);
        }
        setSplitClientRect(flag);
        updateSplitOutlinePosition();
    }
    
    void SplitOutline::updateSplitOutlinePosition()
    {
        if (clientsStatus() == 1)
        {
            setGeometry((m_workspaceRect.x() + m_workspaceRect.width()/2) - splitoutlinePos, m_workspaceRect.y(), splitoutlineWidth, m_workspaceRect.height());
            move(m_leftSplitClient->x() + m_leftSplitClient->width() - splitoutlinePos, m_workspaceRect.y());
            update();
        }
    }

    AbstractClient* SplitOutline::getLeftSplitClient()
    {
        return  m_leftSplitClient;
    }

    AbstractClient* SplitOutline::getRightSplitClient()
    {
        return m_rightSplitClient;
    }
    
    QRect SplitOutline::getLeftSplitClientRect()
    {
        return  m_leftSplitClientRect;
    }

    QRect SplitOutline::getRightSplitClientRect()
    {
        return m_rightSplitClientRect;
    }

    void SplitOutline::setSplitClientRect(QuickTileFlag flag)
    {
        if (m_leftSplitClient && flag == QuickTileFlag::Left) {
            m_leftSplitClientRect = m_leftSplitClient->geometry();
        } else if (m_rightSplitClient && flag == QuickTileFlag::Right) {
            m_rightSplitClientRect = m_rightSplitClient->geometry();
        }
    }
    
    void SplitOutline::resumeClientLocation()
    {
         if ((m_rightSplitClientRect.isEmpty() || m_leftSplitClientRect.isEmpty()) && !m_workspaceRect.isEmpty()) {
             m_leftSplitClientRect = QRect(m_workspaceRect.x(), m_workspaceRect.y(), m_workspaceRect.width()/2, m_workspaceRect.height());
             m_rightSplitClientRect = QRect(m_workspaceRect.x() + m_workspaceRect.width()/2, 0, m_workspaceRect.width()/2, m_workspaceRect.height());
         }

         m_leftSplitClient->setGeometry(m_leftSplitClientRect.x(), m_workspaceRect.y(), m_leftSplitClientRect.width(), m_workspaceRect.height());
         m_leftSplitClient->palette();
         m_rightSplitClient->setGeometry(m_rightSplitClientRect.x(), m_workspaceRect.y(), m_rightSplitClientRect.width(), m_workspaceRect.height());
         m_rightSplitClient->palette();
         updateSplitOutlinePosition();
    }

    void SplitOutline::swapClientLocation()
    {
        AbstractClient *p = m_leftSplitClient;
        setSplitClient(m_rightSplitClient, QuickTileFlag::Left);
        setSplitClient(p, QuickTileFlag::Right);
    }

    void SplitOutline::noActiveHide()
    {
        if (clientsStatus() == 1 && isVisible())
        {
            hide();
        }
    }

    void SplitOutline::activeShow()
    {
        if (m_leftSplitClient != nullptr && m_rightSplitClient != nullptr && !isVisible()) {
            show();
            updateSplitOutlinePosition();
        }
    }

    void SplitOutline::updateWorkspaceArea()
    {
        m_workspaceRect = workspace()->clientArea(MaximizeArea, m_screen, m_desktop);
    }

    void SplitOutline::updateLeftRightArea()
    {
        if (clientsStatus() == 1) {
            m_leftSplitClient->setGeometry(m_workspaceRect.x(), m_workspaceRect.y(), m_workspaceRect.width()/2, m_workspaceRect.height());
            m_leftSplitClient->palette();
            m_rightSplitClient->setGeometry(m_workspaceRect.x() + m_workspaceRect.width()/2, m_workspaceRect.y(), m_workspaceRect.width()/2, m_workspaceRect.height());
            m_rightSplitClient->palette();
            updateSplitOutlinePosition();
        }
    }
    
    void SplitOutline::handleDockChangePosition()
    {
        if (clientsStatus() == 1) {
            updateWorkspaceArea();
            m_leftSplitClientRect = m_leftSplitClient->geometry();
            m_rightSplitClientRect = m_rightSplitClient->geometry();
        }
        updateOutlineStatus();
    }

    void SplitOutline::handleSplitScreenLayer()
    {
        if (m_leftSplitClient != nullptr && m_rightSplitClient != nullptr) {
            if (m_leftSplitClient->isActive()) {
                workspace()->raiseClient(m_rightSplitClient);
                workspace()->raiseClient(m_leftSplitClient);
            } else if (m_rightSplitClient->isActive()) {
                workspace()->raiseClient(m_leftSplitClient);
                workspace()->raiseClient(m_rightSplitClient);
            } else {
                workspace()->raiseClient(m_rightSplitClient);
                workspace()->raiseClient(m_leftSplitClient);
            }
        }
    }

    int SplitOutline::clientsStatus()
    {
        if (m_leftSplitClient == nullptr && m_rightSplitClient == nullptr)
            return 0;
        else if (m_leftSplitClient != nullptr && m_rightSplitClient != nullptr)
            return 1;
        else
            return 2;
    }

    bool SplitOutline::clearClient(AbstractClient *client)
    {
        bool flag = false;
        if (m_leftSplitClient == client) {
            m_leftSplitClient = nullptr;
            hide();
            flag = true;
        } else if (m_rightSplitClient == client) {
            m_rightSplitClient = nullptr;
            hide();
            flag = true;
        }

        return flag;
    }

    void SplitOutline::updateOutlineStatus()
    {
        if (clientsStatus() != 1)
            return;
        if (m_cursor != CURSOR_L_R) {
            if (m_leftSplitClientRect.width() > m_rightSplitClientRect.width())
                setCustomCursor(CURSOR_LEFT);
            else if (m_leftSplitClientRect.width() < m_rightSplitClientRect.width())
                setCustomCursor(CURSOR_RIGHT);
            else
                setCustomCursor(CURSOR_L_R);
        }

        updateSplitOutlinePosition();
    }

    bool SplitOutline::isRecordClient(AbstractClient *client)
    {
        if (client != m_leftSplitClient && client != m_rightSplitClient)
            return false;
        return true;
    }

    void SplitOutline::setDefaultCustomCursor()
    {
        setCustomCursor(CURSOR_L_R);
    }

    SplitOutline::~SplitOutline() 
    {

    }
}

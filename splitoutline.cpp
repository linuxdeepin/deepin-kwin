/*
 * Copyright (C) 2017 ~ 2019 Deepin Technology Co., Ltd.
 *
 * Author:     taiyunqiang <taiyunqiang@uniontech.com>
 *
 * Maintainer: taiyunqiang <taiyunqiang@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "splitoutline.h"
#include "workspace.h"
#define CURSOR_LEFT     0
#define CURSOR_RIGHT    1
#define CURSOR_L_R      2

namespace KWin
{
 SplitOutline::SplitOutline()
        : QWidget()
    {
        setWindowFlags(Qt::X11BypassWindowManagerHint);
        setWindowOpacity(0);
        setCustomCursor(CURSOR_L_R); //设置鼠标样式
    }

    void SplitOutline::setCustomCursor(int direct)
    {
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
    }

    int SplitOutline::getCustomCursor()
    {
        return m_cursor;
    }

    void SplitOutline::updateCustomCursorState()
    {
        if (m_leftSplitClientRect.width() == m_rightSplitClientRect.width()) {
            setCustomCursor(CURSOR_L_R);
        }
    }

    void SplitOutline::mousePressEvent(QMouseEvent* e)
    {
        setWindowOpacity(1);
        m_mainWindowPress = true;
        maxLeftSplitClientWidth = m_leftSplitClient->maxSize().width();
        minLeftSplitClientWidth = m_leftSplitClient->minSize().width();
        maxRightSplitClientWidth = m_rightSplitClient->maxSize().width();
        minRightSplitClientWidth = m_rightSplitClient->minSize().width();
    }

    void SplitOutline::mouseMoveEvent(QMouseEvent*e)
    {
        if (m_leftSplitClient != nullptr && m_rightSplitClient != nullptr && m_mainWindowPress == true) {
            const int leftSplitClientWidth = e->screenPos().x() - m_workspaceRect.x();
            const int rightSplitClientWidth = m_workspaceRect.width() - leftSplitClientWidth;

            if ((minLeftSplitClientWidth < leftSplitClientWidth && leftSplitClientWidth < maxLeftSplitClientWidth)
                && (maxRightSplitClientWidth > rightSplitClientWidth && rightSplitClientWidth > minRightSplitClientWidth)) {
                m_leftSplitClient->setGeometry(m_workspaceRect.x(), m_workspaceRect.y(), leftSplitClientWidth, m_workspaceRect.height());
                m_leftSplitClientRect = QRect(m_workspaceRect.x(), m_workspaceRect.y(), leftSplitClientWidth, m_workspaceRect.height());
                m_leftSplitClient->palette();
                m_rightSplitClient->setGeometry(e->screenPos().x(), m_workspaceRect.y(), rightSplitClientWidth, m_workspaceRect.height());
                m_rightSplitClientRect = QRect(e->screenPos().x(), m_workspaceRect.y(), rightSplitClientWidth, m_workspaceRect.height());
                m_rightSplitClient->palette();
                this->move(e->screenPos().x()-10, m_workspaceRect.y());
                setCustomCursor(CURSOR_L_R);
            } else {
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
        setWindowOpacity(0);
    }

    void SplitOutline::enterEvent(QEvent *)
    {
        setWindowOpacity(1);
    }

    void SplitOutline::leaveEvent(QEvent *)
    {
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
        splitOutlinePainter.drawRect(0,0,width(),height());

        QPainter smallLine(this);
        smallLine.setRenderHint(QPainter::Antialiasing, true);
        smallLine.setBrush(QColor("#7A7A7A"));
        smallLine.drawRoundRect(QRect(7, m_workspaceRect.height()/2-25, 6, 50), 30, 30);
    }

    void SplitOutline::setLeftSplitClient(AbstractClient* client)
    {
        m_leftSplitClient = client;
        if (m_leftSplitClient != nullptr && m_rightSplitClient != nullptr && !isVisible())
        {
            updateWorkspaceArea();
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
        if (m_leftSplitClient != nullptr && m_rightSplitClient != nullptr && !isVisible())
        {
            updateWorkspaceArea();
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
        updateSplitOutlinePosition();
    }
    
    void SplitOutline::updateSplitOutlinePosition()
    {
        if (m_leftSplitClient != nullptr && m_rightSplitClient != nullptr && isVisible())
        {
            setGeometry((m_workspaceRect.x() + m_workspaceRect.width()/2)-10, m_workspaceRect.y(), 20, m_workspaceRect.height());
            move(m_leftSplitClient->x() + m_leftSplitClient->width() - 10, m_workspaceRect.y());
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
         if ((m_rightSplitClientRect.isEmpty() || m_leftSplitClientRect.isEmpty()) && !m_workspaceRect.isEmpty()) {
             m_leftSplitClientRect = QRect(m_workspaceRect.x(), m_workspaceRect.y(), m_workspaceRect.width()/2, height());
             m_rightSplitClientRect = QRect(m_workspaceRect.x() + m_workspaceRect.width()/2, m_workspaceRect.y(), m_workspaceRect.width()/2, m_workspaceRect.height());
         }

         m_leftSplitClient->setGeometry(m_workspaceRect.x(), m_workspaceRect.y(), m_rightSplitClientRect.width(), height());
         m_leftSplitClient->palette();
         m_rightSplitClient->setGeometry(m_workspaceRect.x() + m_rightSplitClientRect.width(), m_workspaceRect.y(), m_leftSplitClientRect.width(), m_workspaceRect.height());
         m_rightSplitClient->palette();
         m_leftSplitClientRect = m_leftSplitClient->geometry();
         m_rightSplitClientRect = m_rightSplitClient->geometry();
         updateSplitOutlinePosition();
    }

    void SplitOutline::noActiveHide()
    {
        if (m_leftSplitClient != nullptr && m_rightSplitClient != nullptr && isVisible())
        {
            hide();
        }
    }

    void SplitOutline::activeShow()
    {
        show();
        updateSplitOutlinePosition();
    }

    void SplitOutline::updateWorkspaceArea()
    {
         if (m_leftSplitClient != nullptr && m_rightSplitClient != nullptr) {
             m_workspaceRect = workspace()->clientArea(MaximizeArea, m_leftSplitClient->screen(), m_leftSplitClient->desktop());
         }
    }

    void SplitOutline::updateLeftRightArea()
    {
        if (m_leftSplitClient != nullptr && m_rightSplitClient != nullptr) {
            m_leftSplitClient->setGeometry(m_workspaceRect.x(), m_workspaceRect.y(), m_workspaceRect.width()/2, m_workspaceRect.height());
            m_leftSplitClient->palette();
            m_rightSplitClient->setGeometry(m_workspaceRect.x() + m_workspaceRect.width()/2, m_workspaceRect.y(), m_workspaceRect.width()/2, m_workspaceRect.height());
            m_rightSplitClient->palette();
            updateSplitOutlinePosition();  
        } 
    }
    
    void SplitOutline::handleDockChangePosition()
    {
        updateWorkspaceArea();
        if (m_leftSplitClient != nullptr && m_rightSplitClient != nullptr) {
            m_leftSplitClientRect = m_leftSplitClient->geometry();
            m_rightSplitClientRect = m_rightSplitClient->geometry();
        }
        updateSplitOutlinePosition();
        updateCustomCursorState();
    }


    SplitOutline::~SplitOutline() 
    {

    }
}

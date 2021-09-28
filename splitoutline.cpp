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
namespace KWin
{
 SplitOutline::SplitOutline()
        : QWidget()
    {
        setWindowFlags(Qt::X11BypassWindowManagerHint);
        setWindowOpacity(0);
        setCursor(Qt::SizeHorCursor); //设置鼠标样式
        setStyleSheet("border:1px; border-radius:5px;border-color:#BBBBBB; background-color:#DFDFDF");
        widgetLine = new QWidget(this);
        widgetLine->setGeometry(7,height()/2-25, 6, 50);
        widgetLine->setStyleSheet("border-radius:3px;background-color:#7A7A7A");
        widgetLine->show();
    }

    void SplitOutline::mousePressEvent(QMouseEvent* e)
    {
        setWindowOpacity(1);
        m_mainWindowPress = true;
    }

    void SplitOutline::mouseMoveEvent(QMouseEvent*e)
    {
        if (m_mainWindowPress == true) {
            const int leftSplitClientWidth = e->screenPos().x() - m_workspaceRect.x();
            const int rightSplitClientWidth = m_workspaceRect.width() - leftSplitClientWidth;
            const int maxLeftSplitClientWidth = m_leftSplitClient->maxSize().width();
            const int minLeftSplitClientWidth = m_leftSplitClient->minSize().width();
            const int maxRightSplitClientWidth = m_rightSplitClient->maxSize().width();
            const int minRightSplitClientWidth = m_rightSplitClient->minSize().width();
            
            if (m_leftSplitClient != nullptr && (minLeftSplitClientWidth <= leftSplitClientWidth) && (leftSplitClientWidth <= maxLeftSplitClientWidth)
                                             && (minRightSplitClientWidth <= rightSplitClientWidth) && (rightSplitClientWidth <= maxRightSplitClientWidth)) {
                m_leftSplitClient->setGeometry(m_workspaceRect.x(), 0, leftSplitClientWidth, height());
                m_leftSplitClientRect = QRect(m_workspaceRect.x(), 0, leftSplitClientWidth, height());
                m_leftSplitClient->palette();
                this->move(e->screenPos().x()-10, 0);
            }

            if (m_rightSplitClient != nullptr &&  (minLeftSplitClientWidth <= leftSplitClientWidth) && (leftSplitClientWidth <= maxLeftSplitClientWidth)
                                              &&  (minRightSplitClientWidth <= rightSplitClientWidth) && (rightSplitClientWidth <= maxRightSplitClientWidth)) {
                m_rightSplitClient->setGeometry(e->screenPos().x(), 0, rightSplitClientWidth, height());
                m_rightSplitClientRect = QRect(e->screenPos().x(), 0, rightSplitClientWidth, height());
                m_rightSplitClient->palette();
                this->move(e->screenPos().x()-10, 0);
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
        setWindowOpacity(0.5);
    }

    void SplitOutline::leaveEvent(QEvent *)
    {
        setWindowOpacity(0);
    }

    void SplitOutline::setLeftSplitClient(AbstractClient* client)
    {
        m_leftSplitClient = client;
        if (m_leftSplitClient != nullptr && m_rightSplitClient != nullptr && !isVisible())
        {
            setGeometry((m_workspaceRect.x() + m_workspaceRect.width()/2)-10, 0, 20, m_workspaceRect.height());
            show();
            updateSplitOutlinePosition();
        }
        if (client == nullptr && isVisible()) {
            hide();
        }
    }

    void SplitOutline::setRightSplitClient(AbstractClient* client)
    {
        m_rightSplitClient = client;
        if (m_leftSplitClient != nullptr && m_rightSplitClient != nullptr && !isVisible())
        {
            setGeometry((m_workspaceRect.x() + m_workspaceRect.width()/2)-10, 0, 20, m_workspaceRect.height());
            show();
            updateSplitOutlinePosition();
        }
        if (client == nullptr && isVisible()) {
            hide();
        }
    }

    void SplitOutline::setSplitClient(AbstractClient* client, QuickTileFlag flag)
    {
        if (flag == QuickTileFlag::Left) {
            setLeftSplitClient(client);
        } else if (flag == QuickTileFlag::Right) {
            setRightSplitClient(client);
        }
    }
    
    void SplitOutline::updateSplitOutlinePosition()
    {
        if (m_leftSplitClient != nullptr && m_rightSplitClient != nullptr && isVisible())
        {
            move(m_leftSplitClient->x() + m_leftSplitClient->width() - 10, 0);
            widgetLine->setGeometry(7,height()/2-25, 6, 50);
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
         if (m_rightSplitClientRect.isEmpty() && m_leftSplitClientRect.isEmpty() && !m_workspaceRect.isEmpty()) {
             m_leftSplitClientRect = QRect(m_workspaceRect.x(), 0, m_workspaceRect.width()/2, height());
             m_rightSplitClientRect = QRect(m_workspaceRect.x() + m_workspaceRect.width()/2, 0, m_workspaceRect.width()/2, height());
         }

         m_leftSplitClient->setGeometry(m_leftSplitClientRect.x(), 0, m_leftSplitClientRect.width(), height());
         m_leftSplitClient->palette();
         m_rightSplitClient->setGeometry(m_rightSplitClientRect.x(), 0, m_rightSplitClientRect.width(), height());
         m_rightSplitClient->palette();
         updateSplitOutlinePosition();
    }

    void SplitOutline::swapClientLocation()
    {
         if (m_rightSplitClientRect.isEmpty() && m_leftSplitClientRect.isEmpty() && !m_workspaceRect.isEmpty()) {
             m_leftSplitClientRect = QRect(m_workspaceRect.x(), 0, m_workspaceRect.width()/2, height());
             m_rightSplitClientRect = QRect(m_workspaceRect.x() + m_workspaceRect.width()/2, 0, m_workspaceRect.width()/2, height());
         }

         m_leftSplitClient->setGeometry(0, 0, m_rightSplitClientRect.width(), height());
         m_leftSplitClient->palette();
         m_rightSplitClient->setGeometry(m_rightSplitClientRect.width(), 0, m_leftSplitClientRect.width(), height());
         m_rightSplitClient->palette();
         m_leftSplitClientRect = m_leftSplitClient->geometry();
         m_rightSplitClientRect = m_rightSplitClient->geometry();
         updateSplitOutlinePosition();
    }

    void SplitOutline::setSplitOutlineRect(QRect rect)
    {
        m_workspaceRect = rect;
    }

    SplitOutline::~SplitOutline() 
    {

    }
}

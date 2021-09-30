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
#ifndef SPLITOUTLINE_H
#define SPLITOUTLINE_H

#include <QMouseEvent>
#include <QWidget>

#include "abstract_client.h"

namespace KWin {
class AbstractClient;
class SplitOutline : public QWidget
{
public:

    explicit SplitOutline();

    void mousePressEvent(QMouseEvent* e);

    void mouseMoveEvent(QMouseEvent*e);

    void mouseReleaseEvent(QMouseEvent* e);

    void enterEvent(QEvent *);

    void leaveEvent(QEvent *);
    
    void setSplitClient(AbstractClient* client, QuickTileFlag flag);

    void handleDockChangePosition();
    
    void updateSplitOutlinePosition();

    AbstractClient* getLeftSplitClient();

    AbstractClient* getRightSplitClient();

    QRect getLeftSplitClientRect();

    QRect getRightSplitClientRect();

    void resumeClientLocation();

    void swapClientLocation();

    void activeShow();
    
    void noActiveHide();

     ~SplitOutline();
protected:
    void setLeftSplitClient(AbstractClient* client);

    void setRightSplitClient(AbstractClient* client);

    void setCustomCursor(int direct);

    void updateWorkspaceArea();

    void updateLeftRightArea();

private:
    bool m_mainWindowPress = false;
    AbstractClient* m_leftSplitClient = nullptr;
    AbstractClient* m_rightSplitClient = nullptr;
    QRect m_workspaceRect;
    QRect m_leftSplitClientRect;
    QRect m_rightSplitClientRect;
    QWidget* widgetLine = nullptr;

    int maxLeftSplitClientWidth = 0;
    int minLeftSplitClientWidth = 0;
    int maxRightSplitClientWidth = 0;
    int minRightSplitClientWidth = 0;
};
}
#endif // SPLITOUTLINE_H

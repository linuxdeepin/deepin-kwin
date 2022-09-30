// Copyright (C) 2017 ~ 2019 Deepin Technology Co., Ltd.
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SPLITOUTLINE_H
#define SPLITOUTLINE_H

#include <QMouseEvent>
#include <QWidget>
#include <QPainter>

#include "abstract_client.h"
#include "effects.h"

namespace KWin {
class AbstractClient;
class SplitOutline : public QWidget
{
    Q_OBJECT
public:

    explicit SplitOutline(int screen, int desktop);

    void mousePressEvent(QMouseEvent* e);

    void mouseMoveEvent(QMouseEvent*e);

    void mouseReleaseEvent(QMouseEvent* e);

    void enterEvent(QEvent *);

    void leaveEvent(QEvent *);

    void paintEvent(QPaintEvent *event);
    
    void setSplitClient(AbstractClient* client, QuickTileFlag flag);

    void handleDockChangePosition();
    
    void handleSplitScreenLayer();

    void updateSplitOutlinePosition();

    AbstractClient* getLeftSplitClient();

    AbstractClient* getRightSplitClient();

    QRect getLeftSplitClientRect();

    QRect getRightSplitClientRect();

    void setSplitClientRect(QuickTileFlag flag);

    void resumeClientLocation();

    void swapClientLocation();

    void activeShow();
    
    void noActiveHide();

    int clientsStatus();

    bool clearClient(AbstractClient *client);

    void updateOutlineStatus();

    bool isRecordClient(AbstractClient *client);

     ~SplitOutline();
protected:
    void setLeftSplitClient(AbstractClient* client);

    void setRightSplitClient(AbstractClient* client);

    void setCustomCursor(int direct);

    int getCustomCursor();

    void updateWorkspaceArea();

    void updateLeftRightArea();

private:
    bool m_mainWindowPress = false;
    AbstractClient* m_leftSplitClient = nullptr;
    AbstractClient* m_rightSplitClient = nullptr;
    QRect m_workspaceRect;
    QRect m_leftSplitClientRect;
    QRect m_rightSplitClientRect;

    int m_cursor = 2;
    int m_desktop = 0;
    int m_screen = 0;

    int maxLeftSplitClientWidth = 0;
    int minLeftSplitClientWidth = 0;
    int maxRightSplitClientWidth = 0;
    int minRightSplitClientWidth = 0;

    int m_pos = 0;
    int m_splitOutlineCursorStatus = 2;
    bool m_mouseLeave = true;
};
}
#endif // SPLITOUTLINE_H

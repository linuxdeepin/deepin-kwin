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
#include <QPainter>

#include "abstract_client.h"
#include "effects.h"

namespace KWin {
class AbstractClient;
class SplitOutline : public QWidget
{
    Q_OBJECT
public:
    static SplitOutline *instance();

    explicit SplitOutline();

    void mousePressEvent(QMouseEvent* e) override;

    void mouseMoveEvent(QMouseEvent*e) override;

    void mouseReleaseEvent(QMouseEvent* e) override;

    void enterEvent(QEvent *) override;

    void leaveEvent(QEvent *) override;

    void paintEvent(QPaintEvent *event) override;

    void handleDockChangePosition();
    
    void handleSplitScreenLayer();

    void updateSplitOutlinePosition();

    void resumeClientLocation();

    void updateOutlineStatus();

    void setShowPos(int pos, bool isLeftRight);

    void showOutline(QRect rect);
    void hideOutline();
    void resetPresStatus() {m_mainWindowPress = false;}

     ~SplitOutline();
protected:
    void setCustomCursor(int direct);

    int getCustomCursor();

    void updateWorkspaceArea();

private:
    static SplitOutline *m_instance;

    bool m_mainWindowPress = false;
    QuickTileMode m_activeClientMode = QuickTileMode(QuickTileFlag::None);
    bool m_isTopDownMove = false;
    AbstractClient* m_activeClient = nullptr;
    AbstractClient* m_leftSplitClient = nullptr;
    AbstractClient* m_rightSplitClient = nullptr;
    QRect m_workspaceRect;
    QRect m_Rect;

    int m_cursor = 4;
    int m_desktop = 1;
    QString m_screen;

    int maxLeftSplitClientWidth = 0;
    int minLeftSplitClientWidth = 0;
    int maxRightSplitClientWidth = 0;
    int minRightSplitClientWidth = 0;
    int m_pos = 0;
    int m_splitOutlineCursorStatus = 2;
    int m_showPos = 0;
};
}
#endif // SPLITOUTLINE_H

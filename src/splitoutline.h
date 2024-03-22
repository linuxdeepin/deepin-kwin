// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
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
    static void clearUpInstance();

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

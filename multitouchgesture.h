// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef MULTITOUCHGESTURE_H
#define MULTITOUCHGESTURE_H
#include <QObject>
#include <QTimer>

namespace KWin
{

typedef struct TPoint {
    double x;
    double y;
}TPoint;

typedef struct ScreenInfo { // touchscreen info
    int x;
    int y;
    int width;
    int height;
} ScreenInfo;

typedef struct Movement {
    TPoint start;
    unsigned int startTime;
    TPoint end;
    unsigned int endTime;
    bool ready;
    bool down;
} Movement;

typedef struct MoveStop {
    double x, y;
    unsigned int startTime;
} MoveStop;

enum Direction {
    DIR_NONE,
    DIR_TOP,          // movement towards top
    DIR_RIGHT,        // movement towards right
    DIR_BOT,          // movement towards bottom
    DIR_LEFT,         // movement towards left
};

enum Position {
    NONE,
    BOTTOM,
    TOP,
    LEFT,
    RIGHT,
};

typedef struct DockInfo {
    Position position;
    int      directionHeight;

    void reset() {
        position = Position::NONE;
        directionHeight = 0;
    }
} DockInfo;

typedef enum Direction Direction;

class MultiTouchGesture : public QObject
{
    Q_OBJECT
public:
    MultiTouchGesture(QObject* parent = nullptr);
    virtual ~MultiTouchGesture();

    static MultiTouchGesture* instance() {
        static MultiTouchGesture gestureRecognizer;
        return &gestureRecognizer;
    }

    void handleTouchDown(double x, double y, unsigned int time);
    void handleTouchMotion(double x, double y, unsigned int time);
    void handleTouchUp(unsigned int time);

signals:
    void showMultitasking();

private:
    void getScreenInfo();

    const DockInfo& getDockInfo();

    void initMoveStop();

    int isValidMoveStopTouch(double x, double y);
    void updateMoveStop();

    Direction discernDirection(TPoint start);
    double distanceEuclidian(TPoint a, TPoint b);
    double movementLength();
    Direction fromEdgeMoveStopDirection();

    int getCurrentFingersCount();

    int            m_fingersCount;
    Movement       m_movement;
    MoveStop       m_moveStop;
    ScreenInfo     m_screenInfo;
    DockInfo       m_dockInfo;
    QTimer*        m_timer;
};

}

#endif // MULTITOUCHGESTURE_H

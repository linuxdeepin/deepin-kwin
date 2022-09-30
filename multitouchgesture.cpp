// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "multitouchgesture.h"
#include <math.h>
#include "screens.h"

#include "workspace.h"
#include "abstract_client.h"

namespace KWin
{

static const int edgeErrorLimit = 3;        // edge error limit when swipe to touchscreen from edge
static const double minEdgeDistance = 10.0; // minimum gesture distance from edge (in mm)
static const int moveStopStayTime = 0;      // move stop stay time
static const int moveStopDistance = 1;      // valid move stop distance

static const int margin = 10;               // margin distance in dock fashion mode

MultiTouchGesture::MultiTouchGesture(QObject* parent)
    : QObject(parent)
{
    m_fingersCount = 0;
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [=]() {
        emit this->showMultitasking();
    });
}

MultiTouchGesture::~MultiTouchGesture()
{

}

void MultiTouchGesture::handleTouchDown(double x, double y, unsigned int time)
{
    if (++m_fingersCount != 1)
        return;

    m_movement.start.x = x;
    m_movement.start.y = y;
    m_movement.startTime = time;

    m_movement.end.x = x;
    m_movement.end.y = y;
    m_movement.endTime = time;

    m_movement.down = true;

    initMoveStop();
}

void MultiTouchGesture::handleTouchMotion(double x, double y, unsigned int time)
{
    if (m_fingersCount != 1)
        return;

    m_movement.end.x = x;
    m_movement.end.y = y;
    m_movement.endTime = time;

    updateMoveStop();
}

void MultiTouchGesture::handleTouchUp(unsigned int time)
{
    if (m_fingersCount-- != 1) { // only deal with a single finger
        return;
    }

    m_movement.endTime = time;
    unsigned int stayTime = (m_movement.endTime - m_moveStop.startTime);
    Direction direction =  fromEdgeMoveStopDirection();
    if (direction != DIR_NONE) {
        const DockInfo& dockInfo = getDockInfo();
        float moveDistance = 0;
        if (direction == Direction::DIR_BOT) {
            moveDistance = fabs(static_cast<float>(m_movement.end.y - m_movement.start.y));
        }
        //fprintf(stdout, "moveStopStayTime: %d\n", moveStopStayTime);
        if (moveDistance > static_cast<float>(dockInfo.directionHeight) && stayTime > moveStopStayTime) {
            m_timer->setSingleShot(true);
            m_timer->start(10);
        }
    }
    m_movement.down = false;
}

void MultiTouchGesture::getScreenInfo()
{
    m_screenInfo.x = KWin::screens()->geometry().x();
    m_screenInfo.y = KWin::screens()->geometry().y();
    m_screenInfo.width = KWin::screens()->geometry().width();
    m_screenInfo.height = KWin::screens()->geometry().height();
}

const DockInfo& MultiTouchGesture::getDockInfo()
{
    m_dockInfo.reset();

    KWin::AbstractClient* dockClient = nullptr;
    foreach (AbstractClient *c, workspace()->allClientList()) {
        if (c->isDock()) {
            dockClient = c;
            break;
        }
    }

    if (!dockClient) {
        return m_dockInfo;
    }

    getScreenInfo();

    QPoint pos = dockClient->pos();
    int dockHeight = dockClient->height();
    int dockWidth = dockClient->width();

    if (pos.x() == m_screenInfo.x || pos.x() == m_screenInfo.x + margin) {
        if (pos.y() == m_screenInfo.y || pos.y() == m_screenInfo.y + margin) {
            if (dockWidth == m_screenInfo.width || dockWidth == m_screenInfo.width - 2*margin) {
                m_dockInfo.position = Position::TOP;
                m_dockInfo.directionHeight = dockHeight;
            } else if (dockHeight == m_screenInfo.height || dockHeight == m_screenInfo.height - 2*margin) {
                m_dockInfo.position = Position::LEFT;
                m_dockInfo.directionHeight = dockWidth;
            }
        } else if (pos.y() == m_screenInfo.height - dockHeight || pos.y() == m_screenInfo.height - dockHeight - margin) {
            if (dockWidth == m_screenInfo.width || dockWidth == m_screenInfo.width - 2*margin) {
                m_dockInfo.position = Position::BOTTOM;
                m_dockInfo.directionHeight = dockHeight;
            }
        }
    } else if (pos.x() == m_screenInfo.width - dockWidth || pos.x() == m_screenInfo.width - dockWidth - margin) {
        if (dockHeight == m_screenInfo.height || dockHeight == m_screenInfo.height - 2*margin) {
            m_dockInfo.position = Position::RIGHT;
            m_dockInfo.directionHeight = dockWidth;
        }
    }

    return m_dockInfo;
}

void MultiTouchGesture::initMoveStop()
{
    m_moveStop.x = m_movement.start.x;
    m_moveStop.y = m_movement.start.y;
    m_moveStop.startTime = m_movement.startTime;
}

//valid touch distance when fingers stop on screen
int MultiTouchGesture::isValidMoveStopTouch(double x, double y)
{
    double dx = x - m_moveStop.x;
    double dy = y - m_moveStop.y;
    return fabs(dx) < moveStopDistance && fabs(dy) < moveStopDistance;
}

void MultiTouchGesture::updateMoveStop()
{
    if (!isValidMoveStopTouch(m_movement.end.x, m_movement.end.y)) {
        m_moveStop.x = m_movement.end.x;
        m_moveStop.y = m_movement.end.y;
        m_moveStop.startTime = m_movement.endTime;
    }
}

//discern direction
enum Direction MultiTouchGesture::discernDirection(TPoint start)
{
    if (start.x <= edgeErrorLimit) {
        return DIR_LEFT;
    }
    if (start.x >= m_screenInfo.width - edgeErrorLimit) {
        return DIR_RIGHT;
    }
    if (start.y <= edgeErrorLimit) {
        return DIR_TOP;
    }
    if (start.y >= m_screenInfo.height - edgeErrorLimit) {
        return DIR_BOT;
    }
    return DIR_NONE;
}

double MultiTouchGesture::distanceEuclidian(TPoint a, TPoint b)
{
    return sqrt(pow((a.x - b.x), 2.0) + pow((a.y - b.y), 2.0));
}

double MultiTouchGesture::movementLength()
{
    return distanceEuclidian(m_movement.end, m_movement.start);
}

enum Direction MultiTouchGesture::fromEdgeMoveStopDirection()
{
    if (m_movement.down && (movementLength() >= minEdgeDistance)) {
        return discernDirection(m_movement.start);
    }
    return DIR_NONE;
}

int MultiTouchGesture::getCurrentFingersCount()
{
    int num = 0;
    if (m_movement.down) {
        num++;
    }
    return num;
}

}

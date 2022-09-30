// Copyright (C) 1997 to 2002 Cristian Tibirna <tibirna@kde.org>
// Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
// Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_PLACEMENT_H
#define KWIN_PLACEMENT_H
// KWin
#include <kwinglobals.h>
// Qt
#include <QPoint>
#include <QRect>
#include <QList>

class QObject;

namespace KWin
{

class AbstractClient;
class Client;

class KWIN_EXPORT Placement
{
public:
    virtual ~Placement();

    /**
     * Placement policies. How workspace decides the way windows get positioned
     * on the screen. The better the policy, the heavier the resource use.
     * Normally you don't have to worry. What the WM adds to the startup time
     * is nil compared to the creation of the window itself in the memory
     */
    enum Policy {
        NoPlacement, // not really a placement
        Default, // special, means to use the global default
        Unknown, // special, means the function should use its default
        Random,
        Smart,
        Cascade,
        Centered,
        ZeroCornered,
        UnderMouse, // special
        OnMainWindow, // special
        Maximizing
    };

    void place(AbstractClient* c, QRect& area);

    void placeAtRandom(AbstractClient* c, const QRect& area, Policy next = Unknown);
    void placeCascaded(AbstractClient* c, QRect& area, Policy next = Unknown);
    void placeSmart(AbstractClient* c, const QRect& area, Policy next = Unknown);
    void placeMaximizing(AbstractClient* c, QRect& area, Policy next = Unknown);
    void placeCentered(AbstractClient* c, const QRect& area, Policy next = Unknown);
    void placeZeroCornered(AbstractClient* c, const QRect& area, Policy next = Unknown);
    void placeDialog(AbstractClient* c, QRect& area, Policy next = Unknown);
    void placeUtility(AbstractClient* c, QRect& area, Policy next = Unknown);
    void placeOnScreenDisplay(AbstractClient* c, QRect& area);

    void reinitCascading(int desktop);

    /**
     * Cascades all clients on the current desktop
     **/
    void cascadeDesktop();
    /**
     *   Unclutters the current desktop by smart-placing all clients again.
     **/
    void unclutterDesktop();

    static Policy policyFromString(const QString& policy, bool no_special);
    static const char* policyToString(Policy policy);

private:
    void place(AbstractClient* c, QRect& area, Policy policy, Policy nextPlacement = Unknown);
    void placeUnderMouse(AbstractClient* c, QRect& area, Policy next = Unknown);
    void placeOnMainWindow(AbstractClient* c, QRect& area, Policy next = Unknown);
    void placeTransient(AbstractClient *c);
    QRect checkArea(const AbstractClient*c, const QRect& area);

    //CT needed for cascading+
    struct DesktopCascadingInfo {
        QPoint pos;
        int col;
        int row;
    };

    QList<DesktopCascadingInfo> cci;

    KWIN_SINGLETON(Placement)
};

} // namespace

#endif

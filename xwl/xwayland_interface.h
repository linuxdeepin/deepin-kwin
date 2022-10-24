// Copyright 2019 Roman Gilg <subdiff@gmail.com>
// SPDX-FileCopyrightText: 2022 2019 Roman Gilg <subdiff@gmail.com>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_XWL_XWAYLAND_INTERFACE
#define KWIN_XWL_XWAYLAND_INTERFACE

#include <kwinglobals.h>

#include <QObject>
#include <QPoint>

namespace KWin
{
class Toplevel;

namespace Xwl
{
enum class DragEventReply {
    // event should be ignored by the filter
    Ignore,
    // event is filtered out
    Take,
    // event should be handled as a Wayland native one
    Wayland,
};
}

class KWIN_EXPORT XwaylandInterface : public QObject
{
    Q_OBJECT
public:
    static XwaylandInterface *self();

    virtual Xwl::DragEventReply dragMoveFilter(Toplevel *target, QPoint pos) = 0;

protected:
    explicit XwaylandInterface(QObject *parent = nullptr);
    virtual ~XwaylandInterface();
};

inline XwaylandInterface *xwayland() {
    return XwaylandInterface::self();
}

}

#endif

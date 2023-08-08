// Copyright (C) 2013, 2016 Martin Gräßlin <mgraesslin@kde.org>
// Copyright (C) 2018 Roman Gilg <subdiff@gmail.com>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_TOUCH_INPUT_H
#define KWIN_TOUCH_INPUT_H
#include "input.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QPointF>

namespace KWin
{

class InputRedirection;
class Toplevel;

namespace Decoration
{
class DecoratedClientImpl;
}

namespace LibInput
{
class Device;
}

class TouchInputRedirection : public InputDeviceHandler
{
    Q_OBJECT
public:
    explicit TouchInputRedirection(InputRedirection *parent);
    virtual ~TouchInputRedirection();

    bool positionValid() const override;
    bool focusUpdatesBlocked() override;
    void init();

    void processDown(qint32 id, const QPointF &pos, quint32 time, LibInput::Device *device = nullptr);
    void processUp(qint32 id, quint32 time, LibInput::Device *device = nullptr);
    void processMotion(qint32 id, const QPointF &pos, quint32 time, LibInput::Device *device = nullptr);
    void cancel();
    void frame();

    void insertId(quint32 internalId, qint32 kwaylandId);
    void removeId(quint32 internalId);
    qint32 mappedId(quint32 internalId);

    void setDecorationPressId(qint32 id) {
        m_decorationId = id;
    }
    qint32 decorationPressId() const {
        return m_decorationId;
    }
    void setInternalPressId(qint32 id) {
        m_internalId = id;
    }
    qint32 internalPressId() const {
        return m_internalId;
    }

    QPointF position() const override {
        return m_lastPosition;
    }
    void setEnableLongPress(bool flag) {
        m_isEnableLongPress = flag;
    }
    bool isEnableLongPress() {
        return m_isEnableLongPress;
    }

private:
    void handleLongPress();
    void cleanupInternalWindow(QWindow *old, QWindow *now) override;
    void cleanupDecoration(Decoration::DecoratedClientImpl *old, Decoration::DecoratedClientImpl *now) override;

    void focusUpdate(Toplevel *focusOld, Toplevel *focusNow) override;

    bool m_inited = false;
    qint32 m_decorationId = -1;
    qint32 m_internalId = -1;
    bool m_isEnableLongPress = false;
    /**
     * external/kwayland
     **/
    QHash<qint32, qint32> m_idMapper;
    QMetaObject::Connection m_focusGeometryConnection;
    bool m_windowUpdatedInCycle = false;
    QPointF m_lastPosition;

    int m_touches = 0;
    QTimer *m_timer;
};

}

#endif

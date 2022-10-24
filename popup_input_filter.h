// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// Copyright 2017  Martin Graesslin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_POPUP_INPUT_FILTER
#define KWIN_POPUP_INPUT_FILTER

#include "input.h"

#include <QObject>
#include <QVector>

namespace KWin
{
class Toplevel;
class ShellClient;

class PopupInputFilter : public QObject, public InputEventFilter
{
    Q_OBJECT
public:
    explicit PopupInputFilter();
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override;
private:
    void handleClientAdded(Toplevel *client);
    void handleClientRemoved(Toplevel *client);
    void disconnectClient(Toplevel *client);
    void cancelPopups();

    QVector<Toplevel*> m_popupClients;
};
}

#endif

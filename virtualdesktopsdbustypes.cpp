// Copyright (C) 2018 Marco Martin <mart@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

// own
#include "virtualdesktopsdbustypes.h"


// Marshall the DBusDesktopDataStruct data into a D-BUS argument
const QDBusArgument &operator<<(QDBusArgument &argument, const KWin::DBusDesktopDataStruct &desk)
{
    argument.beginStructure();
    argument << desk.position;
    argument << desk.id;
    argument << desk.name;
    argument.endStructure();
    return argument;
}
// Retrieve
const QDBusArgument &operator>>(const QDBusArgument &argument, KWin::DBusDesktopDataStruct &desk)
{
    argument.beginStructure();
    argument >> desk.position;
    argument >> desk.id;
    argument >> desk.name;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator<<(QDBusArgument &argument, const KWin::DBusDesktopDataVector &deskVector)
{
    argument.beginArray(qMetaTypeId<KWin::DBusDesktopDataStruct>());
    for (int i = 0; i < deskVector.size(); ++i) {
        argument << deskVector[i];
    }
    argument.endArray();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, KWin::DBusDesktopDataVector &deskVector)
{
    argument.beginArray();
    deskVector.clear();

    while (!argument.atEnd()) {
        KWin::DBusDesktopDataStruct element;
        argument >> element;
        deskVector.append(element);
    }

    argument.endArray();

    return argument;
}

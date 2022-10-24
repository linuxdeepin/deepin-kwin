// Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
// Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_GROUP_H
#define KWIN_GROUP_H

#include "utils.h"
#include <X11/X.h>
#include <fixx11h.h>
#include <netwm.h>

namespace KWin
{

class Client;
class EffectWindowGroupImpl;

class Group
{
public:
    Group(Window leader);
    ~Group();
    Window leader() const;
    const Client* leaderClient() const;
    Client* leaderClient();
    const ClientList& members() const;
    QIcon icon() const;
    void addMember(Client* member);
    void removeMember(Client* member);
    void gotLeader(Client* leader);
    void lostLeader();
    void updateUserTime(xcb_timestamp_t time);
    xcb_timestamp_t userTime() const;
    void ref();
    void deref();
    EffectWindowGroupImpl* effectGroup();
private:
    void startupIdChanged();
    ClientList _members;
    Client* leader_client;
    Window leader_wid;
    NETWinInfo* leader_info;
    xcb_timestamp_t user_time;
    int refcount;
    EffectWindowGroupImpl* effect_group;
};

inline Window Group::leader() const
{
    return leader_wid;
}

inline const Client* Group::leaderClient() const
{
    return leader_client;
}

inline Client* Group::leaderClient()
{
    return leader_client;
}

inline const ClientList& Group::members() const
{
    return _members;
}

inline xcb_timestamp_t Group::userTime() const
{
    return user_time;
}

inline
EffectWindowGroupImpl* Group::effectGroup()
{
    return effect_group;
}

} // namespace

#endif

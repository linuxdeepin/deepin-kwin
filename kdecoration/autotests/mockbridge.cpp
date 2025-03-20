/*
 * SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
 */
#include "mockbridge.h"
#include "mockclient.h"
#include "mocksettings.h"
#include <QtGlobal>

std::unique_ptr<KDecoration2::DecoratedClientPrivate> MockBridge::createClient(KDecoration2::DecoratedClient *client, KDecoration2::Decoration *decoration)
{
    auto ptr = std::unique_ptr<MockClient>(new MockClient(client, decoration));
    m_lastCreatedClient = ptr.get();
    return std::move(ptr);
}

std::unique_ptr<KDecoration2::DecorationSettingsPrivate> MockBridge::settings(KDecoration2::DecorationSettings *parent)
{
    auto ptr = std::unique_ptr<MockSettings>(new MockSettings(parent));
    m_lastCreatedSettings = ptr.get();
    return std::move(ptr);
}

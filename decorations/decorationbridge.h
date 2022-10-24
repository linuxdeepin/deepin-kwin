// Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_DECORATION_BRIDGE_H
#define KWIN_DECORATION_BRIDGE_H

#include <kwinglobals.h>

#include <KDecoration2/Private/DecorationBridge>

#include <KSharedConfig>

#include <QObject>
#include <QSharedPointer>

class KPluginFactory;

namespace KDecoration2
{
class DecorationSettings;
}

namespace KWin
{

class AbstractClient;

namespace Decoration
{

class DecorationBridge : public KDecoration2::DecorationBridge
{
    Q_OBJECT
public:
    virtual ~DecorationBridge();

    void init();
    KDecoration2::Decoration *createDecoration(AbstractClient *client);

    std::unique_ptr<KDecoration2::DecoratedClientPrivate> createClient(KDecoration2::DecoratedClient *client, KDecoration2::Decoration *decoration) override;
    std::unique_ptr<KDecoration2::DecorationSettingsPrivate> settings(KDecoration2::DecorationSettings *parent) override;
    void update(KDecoration2::Decoration *decoration, const QRect &geometry) override;

    bool needsBlur() const {
        return m_blur;
    }

    void reconfigure();

    const QSharedPointer<KDecoration2::DecorationSettings> &settings() const {
        return m_settings;
    }

    QString supportInformation() const;

private:
    QString readPlugin();
    void loadMetaData(const QJsonObject &object);
    void findTheme(const QVariantMap &map);
    void initPlugin();
    QString readTheme() const;
    KPluginFactory *m_factory;
    KSharedConfig::Ptr m_lnfConfig;
    bool m_blur;
    QString m_plugin;
    QString m_defaultTheme;
    QString m_theme;
    QSharedPointer<KDecoration2::DecorationSettings> m_settings;
    bool m_noPlugin;
    KWIN_SINGLETON(DecorationBridge)
};
} // Decoration
} // KWin

#endif

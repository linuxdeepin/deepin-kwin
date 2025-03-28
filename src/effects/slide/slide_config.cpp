/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017, 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "slide_config.h"

#include <config-kwin.h>

// KConfigSkeleton
#include "slideconfig.h"

#include <kwineffects_interface.h>

#include <KPluginFactory>

K_PLUGIN_CLASS(KWin::SlideEffectConfig)

namespace KWin
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
SlideEffectConfig::SlideEffectConfig(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
{
    m_ui.setupUi(this);
    SlideConfig::instance(KWIN_CONFIG);
    addConfig(SlideConfig::self(), this);
}
#else
SlideEffectConfig::SlideEffectConfig(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data)
{
    m_ui.setupUi(widget());
    SlideConfig::instance(KWIN_CONFIG);
    addConfig(SlideConfig::self(), widget());
}
#endif

SlideEffectConfig::~SlideEffectConfig()
{
}

void SlideEffectConfig::save()
{
    KCModule::save();

    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("slide"));
}

} // namespace KWin

#include "slide_config.moc"

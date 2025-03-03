/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KCModule>

#include "ui_diminactive_config.h"

namespace KWin
{

class DimInactiveEffectConfig : public KCModule
{
    Q_OBJECT

public:
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    explicit DimInactiveEffectConfig(QWidget *parent = nullptr, const QVariantList &args = QVariantList());
#else
    explicit DimInactiveEffectConfig(QObject *parent, const KPluginMetaData &data);
#endif

    ~DimInactiveEffectConfig() override;

    void save() override;

private:
    ::Ui::DimInactiveEffectConfig m_ui;
};

} // namespace KWin

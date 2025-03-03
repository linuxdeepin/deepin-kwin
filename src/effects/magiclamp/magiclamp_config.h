/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kcmodule.h>

#include "ui_magiclamp_config.h"

namespace KWin
{

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
class MagicLampEffectConfigForm : public QWidget, public Ui::MagicLampEffectConfigForm
{
    Q_OBJECT
public:
    explicit MagicLampEffectConfigForm(QWidget *parent);
};
#endif

class MagicLampEffectConfig : public KCModule
{
    Q_OBJECT
public:
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    explicit MagicLampEffectConfig(QWidget *parent = nullptr, const QVariantList &args = QVariantList());
#else
    explicit MagicLampEffectConfig(QObject *parent, const KPluginMetaData &data);
#endif
public Q_SLOTS:
    void save() override;

private:
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    MagicLampEffectConfigForm m_ui;
#else
    Ui::MagicLampEffectConfigForm m_ui;
#endif
};

} // namespace

/*
 * SPDX-FileCopyrightText: 2021 Alexander Lohnau <alexander.lohnau@gmx.de>
 *
 * SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
 */

#include "decorationthemeprovider.h"

#include <KPluginMetaData>

class DecorationThemeMetaDataPrivate : public QSharedData
{
public:
    QString visibleName;
    QString themeName;
    QString pluginId;
    bool hasConfig = false;
    KDecoration2::BorderSize borderSize = KDecoration2::BorderSize::Normal;
};

using namespace KDecoration2;

DecorationThemeMetaData::DecorationThemeMetaData()
    : d(new DecorationThemeMetaDataPrivate())
{
}

DecorationThemeMetaData::~DecorationThemeMetaData() = default;

QString DecorationThemeMetaData::visibleName() const
{
    return d->visibleName;
}

void DecorationThemeMetaData::setVisibleName(const QString &name)
{
    d->visibleName = name;
}

bool DecorationThemeMetaData::hasConfiguration() const
{
    return d->hasConfig;
}

void DecorationThemeMetaData::setHasConfiguration(bool hasConfig)
{
    d->hasConfig = hasConfig;
}

QString DecorationThemeMetaData::themeName() const
{
    return d->themeName;
}

void DecorationThemeMetaData::setThemeName(const QString &name)
{
    d->themeName = name;
}

void DecorationThemeMetaData::setBorderSize(KDecoration2::BorderSize size)
{
    d->borderSize = size;
}

KDecoration2::BorderSize DecorationThemeMetaData::borderSize() const
{
    return d->borderSize;
}

QString DecorationThemeMetaData::pluginId() const
{
    return d->pluginId;
}

void DecorationThemeMetaData::setPluginId(const QString &id)
{
    d->pluginId = id;
}

DecorationThemeProvider::DecorationThemeProvider(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : QObject(parent)
{
    Q_UNUSED(data)
    Q_UNUSED(args)
}

DecorationThemeMetaData::DecorationThemeMetaData(const DecorationThemeMetaData &other) = default;

DecorationThemeMetaData &DecorationThemeMetaData::operator=(const DecorationThemeMetaData &other) = default;

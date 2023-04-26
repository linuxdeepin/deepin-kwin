// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef CHAMELEONSHADOW_H
#define CHAMELEONSHADOW_H

#include "chameleontheme.h"

#include <KDecoration2/DecorationShadow>

#include <QSharedPointer>

class ChameleonShadow
{
public:
    static ChameleonShadow *instance();

    static QString buildShadowCacheKey(const ChameleonTheme::ThemeConfig &config, qreal scale);
    QSharedPointer<KDecoration2::DecorationShadow> getShadow(const ChameleonTheme::ThemeConfig &config, qreal scale);

    void clearCache();

protected:
    ChameleonShadow();

private:
    QMap<QString, QSharedPointer<KDecoration2::DecorationShadow>> m_shadowCache;
    QSharedPointer<KDecoration2::DecorationShadow> m_emptyShadow;
};

#endif // CHAMELEONSHADOW_H

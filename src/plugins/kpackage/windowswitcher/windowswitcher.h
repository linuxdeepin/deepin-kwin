/*
    SPDX-FileCopyrightText: 2017 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <KPackage/PackageStructure>

class SwitcherPackage : public KPackage::PackageStructure
{
public:
    SwitcherPackage(QObject *, const QVariantList &)
    {
    }
    void initPackage(KPackage::Package *package) override;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    void pathChanged(KPackage::Package *package) override;
#endif
};

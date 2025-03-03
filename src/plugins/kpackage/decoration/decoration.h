/*
    SPDX-FileCopyrightText: 2017 Demitrius Belai <demitriusbelai@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <KPackage/PackageStructure>

class DecorationPackage : public KPackage::PackageStructure
{
public:
    DecorationPackage(QObject *, const QVariantList &)
    {
    }
    void initPackage(KPackage::Package *package) override;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    void pathChanged(KPackage::Package *package) override;
#endif
};

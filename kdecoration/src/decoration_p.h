/*
 * SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
 */
#pragma once
#include "decoration.h"

//
//  W A R N I N G
//  -------------
//
// This file is not part of the KDecoration2 API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

namespace KDecoration2
{
class Decoration;
class DecorationBridge;
class DecorationButton;
class DecoratedClient;
class DecorationSettings;
class DecorationShadow;

class Q_DECL_HIDDEN Decoration::Private
{
public:
    Private(Decoration *decoration, const QVariantList &args);

    QMargins borders;
    QMargins resizeOnlyBorders;

    Qt::WindowFrameSection sectionUnderMouse;
    void setSectionUnderMouse(Qt::WindowFrameSection section);
    void updateSectionUnderMouse(const QPoint &mousePosition);

    QRect titleBar;
    QRegion blurRegion;

    void addButton(DecorationButton *button);

    QSharedPointer<DecorationSettings> settings;
    DecorationBridge *bridge;
    QSharedPointer<DecoratedClient> client;
    bool opaque;
    QVector<DecorationButton *> buttons;
    QSharedPointer<DecorationShadow> shadow;

private:
    Decoration *q;
};

} // namespace

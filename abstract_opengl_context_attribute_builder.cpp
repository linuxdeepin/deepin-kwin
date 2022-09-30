// Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "abstract_opengl_context_attribute_builder.h"

namespace KWin
{

QDebug AbstractOpenGLContextAttributeBuilder::operator<<(QDebug dbg) const
{
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "\nVersion requested:\t" << isVersionRequested() << "\n";
    if (isVersionRequested()) {
        dbg.nospace() << "Version:\t" << majorVersion() << "." << minorVersion() << "\n";
    }
    dbg.nospace() << "Robust:\t" << isRobust() << "\n";
    dbg.nospace() << "Forward compatible:\t" << isForwardCompatible() << "\n";
    dbg.nospace() << "Core profile:\t" << isCoreProfile() << "\n";
    dbg.nospace() << "Compatibility profile:\t" << isCompatibilityProfile() << "\n";
    dbg.nospace() << "High priority:\t" << isHighPriority();
    return dbg;
}

}

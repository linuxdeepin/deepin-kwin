/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "outputconfiguration.h"
#include <QLoggingCategory>
#include "utils/common.h"

namespace KWin
{

void OutputChangeSet::dump() const
{
    qCDebug(KWIN_CORE) << "OutputChangeSet:"
                       << "\n mode->" << mode
                       << "\n enabled->" << enabled
                       << "\n pos->" << pos
                       << "\n scale->" << scale
                       << "\n transform->" << transform
                       << "\n overscan->" << overscan
                       << "\n rgbRange->" << rgbRange
                       << "\n vrrPolicy->" << vrrPolicy
                       << "\n brightness->" << brightness
                       << "\n ctm->" << ctmValue
                       << "\n colorCurves->" << colorCurves
                       << "\n colorMode—>" << colorModeValue;
}

std::shared_ptr<OutputChangeSet> OutputConfiguration::changeSet(Output *output)
{
    auto &ret = m_properties[output];
    if (!ret) {
        ret = std::make_shared<OutputChangeSet>();
    }
    return ret;
}

std::shared_ptr<OutputChangeSet> OutputConfiguration::constChangeSet(Output *output) const
{
    return m_properties[output];
}

void OutputConfiguration::reset() {
    m_properties.clear();
}

}

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
                        << "\n\t mode:" << mode
                        << "\n\t enabled:" << enabled
                        << "\n\t pos:" << pos
                        << "\n\t scale:" << scale
                        << "\n\t transform:" << transform
                        << "\n\t overscan:" << overscan
                        << "\n\t rgbRange:" << rgbRange
                        << "\n\t vrrPolicy:" << vrrPolicy
                        << "\n\t brightness:" << brightness
                        << "\n\t ctm:" << ctmValue
                        << "\n\t colorCurves:" << colorCurves
                        << "\n\t colorMode:" << colorModeValue;
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

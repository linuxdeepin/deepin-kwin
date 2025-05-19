/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "kwin_export.h"

#include "output.h"

namespace KWin
{

struct KWIN_EXPORT OutputChangeSet
{
    std::optional<std::weak_ptr<OutputMode>> mode;
    std::optional<bool> enabled;
    std::optional<QPoint> pos;
    std::optional<float> scale;
    std::optional<Output::Transform> transform;
    std::optional<uint32_t> overscan;
    std::optional<Output::RgbRange> rgbRange;
    std::optional<RenderLoop::VrrPolicy> vrrPolicy;
    std::optional<int32_t> brightness;
    std::optional<Output::CtmValue> ctmValue;
    std::optional<Output::ColorCurves> colorCurves;
    std::optional<Output::ColorMode> colorModeValue;
    void dump() const;
};

class KWIN_EXPORT OutputConfiguration
{
public:
    /**
    * @brief Fetch a writable changeset to record output's configuration
    * @return if no output was found, create a "no value" record in need
    */
    std::shared_ptr<OutputChangeSet> changeSet(Output *output);
    /**
     * @brief Fetch a read-only changeset to retrieve output's configuration
     * @return return nullptr if no output was found
     */
    std::shared_ptr<OutputChangeSet> constChangeSet(Output *output) const;
    void reset();

private:
    QMap<Output *, std::shared_ptr<OutputChangeSet>> m_properties;
};

}

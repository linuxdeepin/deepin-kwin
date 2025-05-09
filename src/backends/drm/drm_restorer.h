/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 huangzhenyan <huangzhenyan@uniontech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <memory>
#include <QObject>

namespace KWin
{

class DrmOutput;
class OutputChangeSet;

/*
 * Automatically restore last changeSet when DPMS changed
 * Only affect value which not retrieve from m_pipieline
 */
class DrmRestorer : public QObject
{
    Q_OBJECT
public:
    enum class FilterFlag
    {
        mode = (1 << 0),
        enabled = (1 << 1),
        pos = (1 << 2),
        scale = (1 << 3),
        transform = (1 << 4),
        overscan = (1 << 5),
        rgbRange = (1 << 6),
        vrrPolicy = (1 << 7),
        brightness = (1 << 8),
        ctmValue = (1 << 9),
        colorCurves = (1 << 10),
        colorModeValue = (1 << 11)
    };
    Q_DECLARE_FLAGS(FilterFlags, DrmRestorer::FilterFlag)

    DrmRestorer(DrmOutput *output);

    bool needRestore() const;
    // Not Reentrant
    std::shared_ptr<OutputChangeSet> fetch();

    void updateFilterRules(const FilterFlags &rules);

public Q_SLOTS:
    void accumulate(const std::shared_ptr<OutputChangeSet>& changeSet);

private:
    // The purpose of adding filters is to avoid disrupting
    // the semantics of the two functions in the OutputConfiguration class
    std::shared_ptr<OutputChangeSet> filterChanges() const;

    DrmOutput *m_output;
    bool m_needRestore = false;
    std::shared_ptr<OutputChangeSet> m_originChangeSet;
    std::optional<FilterFlags> m_rules;
};
}
Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::DrmRestorer::FilterFlags)

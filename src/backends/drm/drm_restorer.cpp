/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 huangzhenyan <huangzhenyan@uniontech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_restorer.h"
#include "drm_output.h"
#include "drm_gpu.h"
#include "core/outputconfiguration.h"

namespace KWin
{

DrmRestorer::DrmRestorer(DrmOutput *output)
    : m_output(output), m_originChangeSet(std::make_shared<OutputChangeSet>())
{
    connect(m_output, &DrmOutput::wakeUp, [this] {
        m_needRestore = true;
        auto filteredChangeSet = filterChanges();
        qCInfo(KWIN_DRM) << "Restoring previous DRM Properties:";
        filteredChangeSet->dump();
        m_output->queueChanges(filteredChangeSet);
        if (m_output->gpu()->testPendingConfiguration() != DrmPipeline::Error::None) {
            qCInfo(KWIN_DRM) << "Restoring previous DRM Properties failed, Reverting...";
            m_output->revertQueuedChanges();
            return;
        }
        m_output->applyQueuedChanges(filteredChangeSet);
    });
}

bool DrmRestorer::needRestore() const
{
    return m_needRestore && m_rules;
}

void DrmRestorer::accumulate(const std::shared_ptr<OutputChangeSet>& changeSet)
{
    Q_ASSERT(m_rules);
    #define ITEM_IS_UPDATED(changeSet, item) \
        if (changeSet->item) { \
            m_originChangeSet->item = changeSet->item; \
        }
    ITEM_IS_UPDATED(changeSet, mode)
    ITEM_IS_UPDATED(changeSet, enabled)
    ITEM_IS_UPDATED(changeSet, pos)
    ITEM_IS_UPDATED(changeSet, scale)
    ITEM_IS_UPDATED(changeSet, transform)
    ITEM_IS_UPDATED(changeSet, overscan)
    ITEM_IS_UPDATED(changeSet, rgbRange)
    ITEM_IS_UPDATED(changeSet, vrrPolicy)
    ITEM_IS_UPDATED(changeSet, brightness)
    ITEM_IS_UPDATED(changeSet, ctmValue)
    ITEM_IS_UPDATED(changeSet, colorCurves)
    ITEM_IS_UPDATED(changeSet, colorModeValue)
    #undef ITEM_IS_UPDATED
}

std::shared_ptr<OutputChangeSet> DrmRestorer::fetch()
{
    Q_ASSERT(m_needRestore);
    m_needRestore = false;
    return m_originChangeSet;
}

void DrmRestorer::updateFilterRules(const FilterFlags &rules)
{
    // It is not allowed to call the optional::reset,
    // otherwise it may cause data inconsistency due to dtor
    m_rules = rules;
}

std::shared_ptr<OutputChangeSet> DrmRestorer::filterChanges() const
{
    Q_ASSERT(m_rules);
    auto filteredChangeSet = std::make_shared<OutputChangeSet>();
    #define ITEM_IS_FILTERED(changeSet, item) \
        if (*(m_rules) & FilterFlag::item) { \
            changeSet->item = m_originChangeSet->item; \
        }
    ITEM_IS_FILTERED(filteredChangeSet, mode)
    ITEM_IS_FILTERED(filteredChangeSet, enabled)
    ITEM_IS_FILTERED(filteredChangeSet, pos)
    ITEM_IS_FILTERED(filteredChangeSet, scale)
    ITEM_IS_FILTERED(filteredChangeSet, transform)
    ITEM_IS_FILTERED(filteredChangeSet, overscan)
    ITEM_IS_FILTERED(filteredChangeSet, rgbRange)
    ITEM_IS_FILTERED(filteredChangeSet, vrrPolicy)
    ITEM_IS_FILTERED(filteredChangeSet, brightness)
    ITEM_IS_FILTERED(filteredChangeSet, ctmValue)
    ITEM_IS_FILTERED(filteredChangeSet, colorCurves)
    ITEM_IS_FILTERED(filteredChangeSet, colorModeValue)
    #undef ITEM_IS_FILTERED
    return filteredChangeSet;
}

}
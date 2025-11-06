/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "abstract_data_source.h"

using namespace KWaylandServer;

AbstractDataSource::AbstractDataSource(QObject *parent)
    : QObject(parent)
{
}

void AbstractDataSource::setExclusiveAction(DataDeviceManagerInterface::DnDAction action)
{
    if (m_exclusiveAction != action) {
        m_exclusiveAction = action;
        Q_EMIT exclusiveActionChanged();
    }
}

std::optional<DataDeviceManagerInterface::DnDAction> AbstractDataSource::exclusiveAction() const
{
    return m_exclusiveAction;
}

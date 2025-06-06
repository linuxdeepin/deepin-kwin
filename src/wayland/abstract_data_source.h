/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwin_export.h"

#include "clientconnection.h"
#include "datadevicemanager_interface.h"

struct wl_client;

namespace KWaylandServer
{
/**
 * @brief The AbstractDataSource class abstracts the data that
 * can be transferred to another client.
 *
 * It loosely maps to DataDeviceInterface
 */

// Anything related to selections are pure virtual, content relating
// to drag and drop has a default implementation

class KWIN_EXPORT AbstractDataSource : public QObject
{
    Q_OBJECT
public:
    enum class SourceType {
        FromWaylandClient,
        FromXClient,
        FromPrimary
    };

    virtual bool isAccepted() const
    {
        return false;
    }

    virtual void accept(const QString &mimeType)
    {
    };
    virtual void requestData(const QString &mimeType, qint32 fd) = 0;
    virtual void cancel() = 0;

    virtual QStringList mimeTypes() const = 0;

    /**
     * @returns The Drag and Drop actions supported by this DataSourceInterface.
     */
    virtual DataDeviceManagerInterface::DnDActions supportedDragAndDropActions() const
    {
        return {};
    };
    virtual DataDeviceManagerInterface::DnDAction selectedDndAction() const
    {
        return DataDeviceManagerInterface::DnDAction::None;
    }
    /**
     * The user performed the drop action during a drag and drop operation.
     */
    virtual void dropPerformed(){};
    /**
     * The drop destination finished interoperating with this data source.
     */
    virtual void dndFinished(){};
    /**
     * This event indicates the @p action selected by the compositor after matching the
     * source/destination side actions. Only one action (or none) will be offered here.
     */
    virtual void dndAction(DataDeviceManagerInterface::DnDAction action)
    {
    };

    /**
     *  Called when a user stops clicking but it is not accepted by a client.
     */
    virtual void dndCancelled()
    {
    }

    virtual wl_client *client() const
    {
        return nullptr;
    };

    /**
     * The pid of the Source endpoint.
     *
     * Please note: if the Source got created with @link Display::createClient @endlink
     * the pid will be identical to the process running the KWayland::Server::Display.
     *
     * @returns The pid of the Source.
     **/
    virtual pid_t processId()
    {
        if (origin_pid > 0) {
            return origin_pid;
        }
        return pid;
    }

    virtual SourceType sourceType()
    {
        return SourceType::FromWaylandClient;
    }

    virtual SourceType extSourceType()
    {
        return sourceType();
    }

    // The pid of the Source
    pid_t pid = 0;
    pid_t origin_pid = -1;

Q_SIGNALS:
    void aboutToBeDestroyed();

    void mimeTypeOffered(const QString &);
    void supportedDragAndDropActionsChanged();

protected:
    explicit AbstractDataSource(QObject *parent = nullptr);
};

}

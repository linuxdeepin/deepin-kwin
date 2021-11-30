/*
    SPDX-FileCopyrightText: 2021 David Redondo <kde@david-redondo.de>
    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef KWIN_XWL_DATASOURCE
#define KWIN_XWL_DATASOURCE

#include <KWayland/Server/abstract_data_source.h>

namespace KWin
{
namespace Xwl
{
class XwlDataSource : public KWayland::Server::AbstractDataSource
{
    Q_OBJECT
public:
    explicit XwlDataSource(KWayland::Server::DataDeviceManagerInterface *parent);
    void requestData(const QString &mimeType, qint32 fd) override;
    void cancel() override;
    QStringList mimeTypes() const override;
    void setMimeTypes(const QStringList &mimeTypes);

    void accept(const QString &mimeType) override;
    KWayland::Server::DataDeviceManagerInterface::DnDActions supportedDragAndDropActions() const override;
    void setSupportedDndActions(KWayland::Server::DataDeviceManagerInterface::DnDActions dndActions);

    void dndAction(KWayland::Server::DataDeviceManagerInterface::DnDAction action) override;

    KWayland::Server::DataDeviceManagerInterface::DnDAction selectedDragAndDropAction() {
        return m_dndAction;
    }

    void dropPerformed() override {
        Q_EMIT dropped();
    }
    void dndFinished() override {
        Q_EMIT finished();
    }
    bool isAccepted() const override;

Q_SIGNALS:
    void dataRequested(const QString &mimeType, qint32 fd);
    void dropped();
    void finished();

private:
    QStringList m_mimeTypes;
    KWayland::Server::DataDeviceManagerInterface::DnDActions m_supportedDndActions;
    KWayland::Server::DataDeviceManagerInterface::DnDAction m_dndAction = KWayland::Server::DataDeviceManagerInterface::DnDAction::None;
    bool m_accepted = false;
};
}
}
#endif

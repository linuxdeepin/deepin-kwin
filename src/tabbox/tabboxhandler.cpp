/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "tabboxhandler.h"

#include <config-kwin.h>

// own
#include "clientmodel.h"
#include "composite.h"
#include "desktopmodel.h"
#include "scripting/scripting.h"
#include "switcheritem.h"
#include "tabbox_logging.h"
#include "utils/xcbutils.h"
#include "wayland_server.h"
#include <kwinglobals.h>
// Qt
#include <QKeyEvent>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>
#include <qpa/qwindowsysteminterface.h>
// KDE
#include <KLocalizedString>
#include <KPackage/Package>
#include <KPackage/PackageLoader>
#include <KProcess>

namespace KWin
{
namespace TabBox
{

class TabBoxHandlerPrivate
{
public:
    TabBoxHandlerPrivate(TabBoxHandler *q);

    ~TabBoxHandlerPrivate();

    /**
     * Updates the current highlight window state
     */
    void updateHighlightWindows();
    /**
     * Ends window highlighting
     */
    void endHighlightWindows(bool abort = false);

    void show();
    QQuickWindow *window() const;
    SwitcherItem *switcherItem() const;

    ClientModel *clientModel() const;
    DesktopModel *desktopModel() const;

    bool isHighlightWindows() const;

    TabBoxHandler *q; // public pointer
    // members
    TabBoxConfig config;
    std::unique_ptr<QQmlContext> m_qmlContext;
    std::unique_ptr<QQmlComponent> m_qmlComponent;
    QObject *m_mainItem;
    QMap<QString, QObject *> m_clientTabBoxes;
    QMap<QString, QObject *> m_desktopTabBoxes;
    ClientModel *m_clientModel;
    DesktopModel *m_desktopModel;
    QModelIndex index;
    QRect m_viewRect;
    /**
     * Indicates if the tabbox is shown.
     */
    bool isShown;
    TabBoxClient *lastRaisedClient, *lastRaisedClientSucc;
    int wheelAngleDelta = 0;
    bool m_lastRaisedClientWasMinimized;

private:
    QObject *createSwitcherItem(bool desktopMode);
};

TabBoxHandlerPrivate::TabBoxHandlerPrivate(TabBoxHandler *q)
    : m_qmlContext()
    , m_qmlComponent(nullptr)
    , m_mainItem(nullptr)
{
    this->q = q;
    isShown = false;
    lastRaisedClient = nullptr;
    lastRaisedClientSucc = nullptr;
    m_lastRaisedClientWasMinimized = false;
    config = TabBoxConfig();
    m_clientModel = new ClientModel(q);
    m_desktopModel = new DesktopModel(q);
}

TabBoxHandlerPrivate::~TabBoxHandlerPrivate()
{
    for (auto it = m_clientTabBoxes.constBegin(); it != m_clientTabBoxes.constEnd(); ++it) {
        delete it.value();
    }
    for (auto it = m_desktopTabBoxes.constBegin(); it != m_desktopTabBoxes.constEnd(); ++it) {
        delete it.value();
    }
}

QQuickWindow *TabBoxHandlerPrivate::window() const
{
    if (!m_mainItem) {
        return nullptr;
    }
    if (QQuickWindow *w = qobject_cast<QQuickWindow *>(m_mainItem)) {
        return w;
    }
    return m_mainItem->findChild<QQuickWindow *>();
}

#ifndef KWIN_UNIT_TEST
SwitcherItem *TabBoxHandlerPrivate::switcherItem() const
{
    if (!m_mainItem) {
        return nullptr;
    }
    if (SwitcherItem *i = qobject_cast<SwitcherItem *>(m_mainItem)) {
        return i;
    } else if (QQuickWindow *w = qobject_cast<QQuickWindow *>(m_mainItem)) {
        return w->contentItem()->findChild<SwitcherItem *>();
    }
    return m_mainItem->findChild<SwitcherItem *>();
}
#endif

ClientModel *TabBoxHandlerPrivate::clientModel() const
{
    return m_clientModel;
}

DesktopModel *TabBoxHandlerPrivate::desktopModel() const
{
    return m_desktopModel;
}

bool TabBoxHandlerPrivate::isHighlightWindows() const
{
    const QQuickWindow *w = window();
    if (w && w->visibility() == QWindow::FullScreen) {
        return false;
    }
    return config.isHighlightWindows();
}

void TabBoxHandlerPrivate::updateHighlightWindows()
{
    if (!isShown || config.tabBoxMode() != TabBoxConfig::ClientTabBox) {
        return;
    }

    TabBoxClient *currentClient = q->client(index);
    QWindow *w = window();

    if (q->isKWinCompositing()) {
        if (lastRaisedClient) {
            q->elevateClient(lastRaisedClient, w, false);
        }
        lastRaisedClient = currentClient;
        // don't elevate desktop
        const auto desktop = q->desktopClient().toStrongRef();
        if (currentClient) {
            q->elevateClient(currentClient, w, true);
        }
    } else {
        if (lastRaisedClient) {
            q->shadeClient(lastRaisedClient, true);
            if (lastRaisedClientSucc) {
                q->restack(lastRaisedClient, lastRaisedClientSucc);
                lastRaisedClient->setMinimized(m_lastRaisedClientWasMinimized);
            }
        }

        lastRaisedClient = currentClient;
        m_lastRaisedClientWasMinimized = currentClient->isMinimized();
        if (lastRaisedClient) {
            q->shadeClient(lastRaisedClient, false);
            // TODO if ( (lastRaisedClientWasMinimized = lastRaisedClient->isMinimized()) )
            //         lastRaisedClient->setMinimized( false );
            TabBoxClientList order = q->stackingOrder();
            int succIdx = order.count() + 1;
            for (int i = 0; i < order.count(); ++i) {
                if (order.at(i).toStrongRef() == lastRaisedClient) {
                    succIdx = i + 1;
                    break;
                }
            }
            lastRaisedClientSucc = (succIdx < order.count()) ? order.at(succIdx).toStrongRef().data() : nullptr;
            q->raiseClient(lastRaisedClient);
            q->activateCurrentClient();
        }
    }

    if (config.isShowTabBox() && w) {
        q->highlightWindows(currentClient, w);
    } else {
        q->highlightWindows(currentClient);
    }
}

void TabBoxHandlerPrivate::endHighlightWindows(bool abort)
{
    TabBoxClient *currentClient = q->client(index);
    if (isHighlightWindows() && q->isKWinCompositing()) {
        const auto stackingOrder = q->stackingOrder();
        for (const QWeakPointer<TabBoxClient> &clientPointer : stackingOrder) {
            if (QSharedPointer<TabBoxClient> client = clientPointer.toStrongRef()) {
                if (client != currentClient) { // to not mess up with wanted ShadeActive/ShadeHover state
                    q->shadeClient(client.data(), true);
                }
            }
        }
    }
    QWindow *w = window();
    if (currentClient) {
        q->elevateClient(currentClient, w, false);
    }
    if (abort && lastRaisedClient && lastRaisedClientSucc) {
        q->restack(lastRaisedClient, lastRaisedClientSucc);
    }
    lastRaisedClient = nullptr;
    lastRaisedClientSucc = nullptr;
    m_lastRaisedClientWasMinimized = false;
    // highlight windows
    q->highlightWindows();
}

#ifndef KWIN_UNIT_TEST
QObject *TabBoxHandlerPrivate::createSwitcherItem(bool desktopMode)
{
    // first try look'n'feel package
    QString file = QStandardPaths::locate(
        QStandardPaths::GenericDataLocation,
        QStringLiteral("plasma/look-and-feel/%1/contents/%2")
            .arg(config.layoutName(),
                 desktopMode ? QStringLiteral("desktopswitcher/DesktopSwitcher.qml") : QStringLiteral("windowswitcher/WindowSwitcher.qml")));
    if (file.isNull()) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        const QString folderName = desktopMode ? QLatin1String("kwin/desktoptabbox/") : QLatin1String("kwin/tabbox/");
        auto findSwitcher = [this, desktopMode, folderName] {
            const QString type = desktopMode ? QStringLiteral("KWin/DesktopSwitcher") : QStringLiteral("KWin/WindowSwitcher");
            auto offers = KPackage::PackageLoader::self()->findPackages(type, folderName,
                                                                        [this](const KPluginMetaData &data) {
                                                                            return data.pluginId().compare(config.layoutName(), Qt::CaseInsensitive) == 0;
                                                                        });
            if (offers.isEmpty()) {
                // load default
                offers = KPackage::PackageLoader::self()->findPackages(type, folderName,
                                                                       [](const KPluginMetaData &data) {
                                                                           return data.pluginId().compare(QStringLiteral("informative"), Qt::CaseInsensitive) == 0;
                                                                       });
                if (offers.isEmpty()) {
                    qCDebug(KWIN_TABBOX) << "could not find default window switcher layout";
                    return KPluginMetaData();
                }
            }
            return offers.first();
        };
        auto service = findSwitcher();
        if (!service.isValid()) {
            return nullptr;
        }
        if (service.value(QStringLiteral("X-Plasma-API")) != QLatin1String("declarativeappletscript")) {
            qCDebug(KWIN_TABBOX) << "Window Switcher Layout is no declarativeappletscript";
            return nullptr;
        }
        auto findScriptFile = [service, folderName] {
            const QString pluginName = service.pluginId();
            const QString scriptName = service.value(QStringLiteral("X-Plasma-MainScript"));
            return QStandardPaths::locate(QStandardPaths::GenericDataLocation, folderName + pluginName + QLatin1String("/contents/") + scriptName);
        };
        file = findScriptFile();
#else
        QString path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QLatin1String("kwin-wayland/tabbox/") + config.layoutName(), QStandardPaths::LocateDirectory);
        if (path.isEmpty()) {
            path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QLatin1String("kwin/tabbox/") + config.layoutName(), QStandardPaths::LocateDirectory);
        }
        if (path.isEmpty()) {
            // load default
            qCWarning(KWIN_TABBOX) << "Could not load window switcher package" << config.layoutName() << ". Falling back to default";
            path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QLatin1String("kwin-wayland/tabbox/") + TabBoxConfig::defaultLayoutName(), QStandardPaths::LocateDirectory);
        }

        KPackage::Package pkg = KPackage::PackageLoader::self()->loadPackage(QStringLiteral("KWin/WindowSwitcher"));
        pkg.setPath(path);
        file = pkg.filePath("mainscript");
#endif
    }

    if (file.isNull()) {
        qCWarning(KWIN_TABBOX) << "Could not find QML file for window switcher";
        return nullptr;
    }
    m_qmlComponent->loadUrl(QUrl::fromLocalFile(file));
    if (m_qmlComponent->isError()) {
        qCWarning(KWIN_TABBOX) << "Component failed to load: " << m_qmlComponent->errors();
        QStringList args;
        args << QStringLiteral("--passivepopup") << i18n("The Window Switcher installation is broken, resources are missing.\n"
                                                         "Contact your distribution about this.")
             << QStringLiteral("20");
        KProcess::startDetached(QStringLiteral("kdialog"), args);
        m_qmlComponent.reset(nullptr);
    } else {
        QObject *object = m_qmlComponent->create(m_qmlContext.get());
        if (desktopMode) {
            m_desktopTabBoxes.insert(config.layoutName(), object);
        } else {
            m_clientTabBoxes.insert(config.layoutName(), object);
        }
        return object;
    }
    return nullptr;
}
#endif

void TabBoxHandlerPrivate::show()
{
#ifndef KWIN_UNIT_TEST
    if (Compositor::self() && Compositor::self()->isOpenGLCompositing()) {
        config.setLayoutName("thumbnail_list");
    } else {
        config.setLayoutName("thumbnail_grid");
    }
    if (!m_qmlContext) {
        qmlRegisterType<SwitcherItem>("org.kde.kwin", 2, 0, "Switcher");
        qmlRegisterType<SwitcherItem>("org.kde.kwin", 3, 0, "TabBoxSwitcher");
        m_qmlContext.reset(new QQmlContext(Scripting::self()->qmlEngine()));
    }
    if (!m_qmlComponent) {
        m_qmlComponent.reset(new QQmlComponent(Scripting::self()->qmlEngine()));
    }
    const bool desktopMode = (config.tabBoxMode() == TabBoxConfig::DesktopTabBox);
    auto findMainItem = [this](const QMap<QString, QObject *> &tabBoxes) -> QObject * {
        auto it = tabBoxes.constFind(config.layoutName());
        if (it != tabBoxes.constEnd()) {
            return it.value();
        }
        return nullptr;
    };
    m_mainItem = nullptr;
    m_mainItem = desktopMode ? findMainItem(m_desktopTabBoxes) : findMainItem(m_clientTabBoxes);
    if (!m_mainItem) {
        m_mainItem = createSwitcherItem(desktopMode);
        if (!m_mainItem) {
            return;
        }
    }
    if (waylandServer() && window()) {
        window()->setProperty("__kwin_switcherwin", true);
    }
    if (SwitcherItem *item = switcherItem()) {
        // In case the model isn't yet set (see below), index will be reset and therefore we
        // need to save the current index row (https://bugs.kde.org/show_bug.cgi?id=333511).
        int indexRow = index.row();
        if (!item->model()) {
            QAbstractItemModel *model = nullptr;
            if (desktopMode) {
                model = desktopModel();
            } else {
                model = clientModel();
            }
            item->setModel(model);
        }
        item->setAllDesktops(config.clientDesktopMode() == TabBoxConfig::AllDesktopsClients);
        item->setCurrentIndex(indexRow);
        item->setNoModifierGrab(q->noModifierGrab());
        item->setViewRect(m_viewRect);
        // everything is prepared, so let's make the whole thing visible
        item->setVisible(true);
    }
    if (QWindow *w = window()) {
        w->setProperty("__kwin_switcherwin", true);
        wheelAngleDelta = 0;
        w->installEventFilter(q);
        // pretend to activate the window to enable accessibility notifications
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        QWindowSystemInterface::handleWindowActivated(w, Qt::TabFocusReason);
#else
        QWindowSystemInterface::handleFocusWindowChanged(w, Qt::TabFocusReason);
#endif
    }
#endif
}

/***********************************************
 * TabBoxHandler
 ***********************************************/

TabBoxHandler::TabBoxHandler(QObject *parent)
    : QObject(parent)
{
    KWin::TabBox::tabBox = this;
    d = new TabBoxHandlerPrivate(this);
}

TabBoxHandler::~TabBoxHandler()
{
    delete d;
}

const KWin::TabBox::TabBoxConfig &TabBoxHandler::config() const
{
    return d->config;
}

void TabBoxHandler::setConfig(const TabBoxConfig &config)
{
    d->config = config;
    Q_EMIT configChanged();
}

void TabBoxHandler::show()
{
    d->isShown = true;
    d->lastRaisedClient = nullptr;
    d->lastRaisedClientSucc = nullptr;
    d->m_lastRaisedClientWasMinimized = false;
    if (d->config.isShowTabBox()) {
        d->show();
    }
    if (d->isHighlightWindows()) {
        // TODO this should be
        // QMetaObject::invokeMethod(this, "initHighlightWindows", Qt::QueuedConnection);
        // but we somehow need to cross > 1 event cycle (likely because of queued invocation in the effects)
        // to ensure the EffectWindow is present when updateHighlightWindows, thus elevating the window/tabbox
        QTimer::singleShot(1, this, &TabBoxHandler::initHighlightWindows);
    }
}

void TabBoxHandler::initHighlightWindows()
{
    if (isKWinCompositing()) {
        const auto stack = stackingOrder();
        for (const QWeakPointer<TabBoxClient> &clientPointer : stack) {
            if (QSharedPointer<TabBoxClient> client = clientPointer.toStrongRef()) {
                shadeClient(client.data(), false);
            }
        }
    }
    d->updateHighlightWindows();
}

void TabBoxHandler::hide(bool abort)
{
    d->isShown = false;
    if (d->isHighlightWindows()) {
        d->endHighlightWindows(abort);
    }
#ifndef KWIN_UNIT_TEST
    if (SwitcherItem *item = d->switcherItem()) {
        item->setVisible(false);
    }
#endif
    if (QQuickWindow *w = d->window()) {
        w->hide();
        w->destroy();
    }
    d->m_mainItem = nullptr;
}

QModelIndex TabBoxHandler::nextPrev(TabBoxConfig::TabBoxSwitchPosition direction) const
{
    QModelIndex ret;
    QAbstractItemModel *model;
    switch (d->config.tabBoxMode()) {
    case TabBoxConfig::ClientTabBox:
        model = d->clientModel();
        break;
    case TabBoxConfig::DesktopTabBox:
        model = d->desktopModel();
        break;
    default:
        Q_UNREACHABLE();
    }

    int viewColumnCount  = d->clientModel()->getViewColumnCount();

    if (direction == TabBoxConfig::Forward) {
        int column = d->index.column() + 1;
        int row = d->index.row();
        if (column == model->columnCount()) {
            column = 0;
            row++;
            if (row == model->rowCount()) {
                row = 0;
            }
        }
        ret = model->index(row, column);
        if (!ret.isValid()) {
            ret = model->index(0, 0);
        }
    } else  if (direction == TabBoxConfig::Backward) {
        int column = d->index.column() - 1;
        int row = d->index.row();
        if (column < 0) {
            column = model->columnCount() - 1;
            row--;
            if (row < 0) {
                row = model->rowCount() - 1;
            }
        }
        ret = model->index(row, column);
        if (!ret.isValid()) {
            row = model->rowCount() - 1;
            for (int i = model->columnCount() - 1; i >= 0; i--) {
                ret = model->index(row, i);
                if (ret.isValid()) {
                    break;
                }
            }
        }
    } else if (direction == TabBoxConfig::Up) {
        if (viewColumnCount != 0) {
            if(d->index.row() / viewColumnCount == 0) {
                ret = model->index(d->index.row(), d->index.column());
            } else {
                int column = d->index.column();
                int row = d->index.row() - viewColumnCount;
                ret = model->index(row, column);
                if (!ret.isValid())
                    ret = model->index(0, 0);
            }
        }
    } else if (direction == TabBoxConfig::Down) {
        if (viewColumnCount != 0) {
            if(d->index.row() / viewColumnCount >= model->rowCount() / viewColumnCount) {
                ret = model->index(d->index.row(), d->index.column());
            } else {
                int column = d->index.column();
                int row = d->index.row() + viewColumnCount;
                ret = model->index(row, column);
                if (!ret.isValid())
                    ret = model->index(column, row - viewColumnCount);
            }
        }
    }

    if (ret.isValid()) {
        return ret;
    } else {
        return d->index;
    }
}

QModelIndex TabBoxHandler::desktopIndex(int desktop) const
{
    if (d->config.tabBoxMode() != TabBoxConfig::DesktopTabBox) {
        return QModelIndex();
    }
    return d->desktopModel()->desktopIndex(desktop);
}

QList<int> TabBoxHandler::desktopList() const
{
    if (d->config.tabBoxMode() != TabBoxConfig::DesktopTabBox) {
        return QList<int>();
    }
    return d->desktopModel()->desktopList();
}

int TabBoxHandler::desktop(const QModelIndex &index) const
{
    if (!index.isValid() || (d->config.tabBoxMode() != TabBoxConfig::DesktopTabBox)) {
        return -1;
    }
    QVariant ret = d->desktopModel()->data(index, DesktopModel::DesktopRole);
    if (ret.isValid()) {
        return ret.toInt();
    } else {
        return -1;
    }
}

void TabBoxHandler::setCurrentIndex(const QModelIndex &index)
{
    if (d->index == index) {
        return;
    }
    if (!index.isValid()) {
        return;
    }
    d->index = index;
    if (d->config.tabBoxMode() == TabBoxConfig::ClientTabBox) {
        if (d->isHighlightWindows()) {
            d->updateHighlightWindows();
        }
    }
    Q_EMIT selectedIndexChanged();
}

const QModelIndex &TabBoxHandler::currentIndex() const
{
    return d->index;
}

void TabBoxHandler::grabbedKeyEvent(QKeyEvent *event) const
{
    if (!d->m_mainItem || !d->window()) {
        return;
    }
    QCoreApplication::sendEvent(d->window(), event);
}

bool TabBoxHandler::containsPos(const QPoint &pos) const
{
    if (!d->m_mainItem) {
        return false;
    }
    QWindow *w = d->window();
    if (w) {
        return w->geometry().contains(pos);
    }
    return false;
}

QModelIndex TabBoxHandler::index(QWeakPointer<KWin::TabBox::TabBoxClient> client) const
{
    return d->clientModel()->index(client);
}

TabBoxClientList TabBoxHandler::clientList() const
{
    if (d->config.tabBoxMode() != TabBoxConfig::ClientTabBox) {
        return TabBoxClientList();
    }
    return d->clientModel()->clientList();
}

TabBoxClient *TabBoxHandler::client(const QModelIndex &index) const
{
    if ((!index.isValid()) || (d->config.tabBoxMode() != TabBoxConfig::ClientTabBox)) {
        return nullptr;
    }
    TabBoxClient *c = static_cast<TabBoxClient *>(
        d->clientModel()->data(index, ClientModel::ClientRole).value<void *>());
    return c;
}

void TabBoxHandler::createModel(bool partialReset)
{
    switch (d->config.tabBoxMode()) {
    case TabBoxConfig::ClientTabBox: {
        d->clientModel()->createClientList(partialReset);
        // TODO: C++11 use lambda function
        bool lastRaised = false;
        bool lastRaisedSucc = false;
        const auto clients = stackingOrder();
        for (const auto &clientPointer : clients) {
            QSharedPointer<TabBoxClient> client = clientPointer.toStrongRef();
            if (!client) {
                continue;
            }
            if (client.data() == d->lastRaisedClient) {
                lastRaised = true;
            }
            if (client.data() == d->lastRaisedClientSucc) {
                lastRaisedSucc = true;
            }
        }
        if (d->lastRaisedClient && !lastRaised) {
            d->lastRaisedClient = nullptr;
        }
        if (d->lastRaisedClientSucc && !lastRaisedSucc) {
            d->lastRaisedClientSucc = nullptr;
        }
        break;
    }
    case TabBoxConfig::DesktopTabBox:
        d->desktopModel()->createDesktopList();
        break;
    }
}

QModelIndex TabBoxHandler::first() const
{
    QAbstractItemModel *model;
    switch (d->config.tabBoxMode()) {
    case TabBoxConfig::ClientTabBox:
        model = d->clientModel();
        break;
    case TabBoxConfig::DesktopTabBox:
        model = d->desktopModel();
        break;
    default:
        Q_UNREACHABLE();
    }
    return model->index(0, 0);
}

bool TabBoxHandler::eventFilter(QObject *watched, QEvent *e)
{
    if (e->type() == QEvent::Wheel && watched == d->window()) {
        QWheelEvent *event = static_cast<QWheelEvent *>(e);
        // On x11 the delta for vertical scrolling might also be on X for whatever reason
        const int delta = std::abs(event->angleDelta().x()) > std::abs(event->angleDelta().y()) ? event->angleDelta().x() : event->angleDelta().y();
        d->wheelAngleDelta += delta;
        while (d->wheelAngleDelta <= -120) {
            d->wheelAngleDelta += 120;
            const QModelIndex index = nextPrev(TabBoxConfig::Forward);
            if (index.isValid()) {
                setCurrentIndex(index);
            }
        }
        while (d->wheelAngleDelta >= 120) {
            d->wheelAngleDelta -= 120;
            const QModelIndex index = nextPrev(TabBoxConfig::Backward);
            if (index.isValid()) {
                setCurrentIndex(index);
            }
        }
        return true;
    }
    // pass on
    return QObject::eventFilter(watched, e);
}

void TabBoxHandler::setViewRect(const QRect &rect)
{
    if (SwitcherItem *item = d->switcherItem())
        item->setViewRect(rect);
    d->m_viewRect = rect;
}

TabBoxHandler *tabBox = nullptr;

TabBoxClient::TabBoxClient()
{
}

TabBoxClient::~TabBoxClient()
{
}

} // namespace TabBox
} // namespace KWin

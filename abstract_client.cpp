/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "abstract_client.h"
#include "decorations/decorationpalette.h"
#include "effects.h"
#include "focuschain.h"
#include "outline.h"
#include "screens.h"
#ifdef KWIN_BUILD_TABBOX
#include "tabbox.h"
#endif
#include "screenedge.h"
#include "tabgroup.h"
#include "workspace.h"

#include "wayland_server.h"
#include <KWayland/Server/plasmawindowmanagement_interface.h>

namespace KWin
{

QHash<QString, std::weak_ptr<Decoration::DecorationPalette>> AbstractClient::s_palettes;
std::shared_ptr<Decoration::DecorationPalette> AbstractClient::s_defaultPalette;

AbstractClient::AbstractClient()
    : Toplevel()
#ifdef KWIN_BUILD_TABBOX
    , m_tabBoxClient(QSharedPointer<TabBox::TabBoxClientImpl>(new TabBox::TabBoxClientImpl(this)))
#endif
    , m_colorScheme(QStringLiteral("kdeglobals"))
{
}

AbstractClient::~AbstractClient()
{
    assert(m_blockGeometryUpdates == 0);
}

void AbstractClient::updateMouseGrab()
{
}

bool AbstractClient::belongToSameApplication(const AbstractClient *c1, const AbstractClient *c2, bool active_hack)
{
    return c1->belongsToSameApplication(c2, active_hack);
}

bool AbstractClient::isTransient() const
{
    return false;
}

TabGroup *AbstractClient::tabGroup() const
{
    return nullptr;
}

bool AbstractClient::untab(const QRect &toGeometry, bool clientRemoved)
{
    Q_UNUSED(toGeometry)
    Q_UNUSED(clientRemoved)
    return false;
}

bool AbstractClient::isCurrentTab() const
{
    return true;
}

void AbstractClient::growHorizontal()
{
}

void AbstractClient::growVertical()
{
}

void AbstractClient::shrinkHorizontal()
{
}

void AbstractClient::shrinkVertical()
{
}

void AbstractClient::packTo(int left, int top)
{
    Q_UNUSED(left)
    Q_UNUSED(top)
}

xcb_timestamp_t AbstractClient::userTime() const
{
    return XCB_TIME_CURRENT_TIME;
}

void AbstractClient::setSkipSwitcher(bool set)
{
    set = rules()->checkSkipSwitcher(set);
    if (set == skipSwitcher())
        return;
    m_skipSwitcher = set;
    updateWindowRules(Rules::SkipSwitcher);
    emit skipSwitcherChanged();
}

void AbstractClient::setSkipPager(bool b)
{
    b = rules()->checkSkipPager(b);
    if (b == skipPager())
        return;
    m_skipPager = b;
    doSetSkipPager();
    info->setState(b ? NET::SkipPager : NET::States(0), NET::SkipPager);
    updateWindowRules(Rules::SkipPager);
    emit skipPagerChanged();
}

void AbstractClient::doSetSkipPager()
{
}

void AbstractClient::setSkipTaskbar(bool b)
{
    int was_wants_tab_focus = wantsTabFocus();
    if (b == skipTaskbar())
        return;
    m_skipTaskbar = b;
    doSetSkipTaskbar();
    updateWindowRules(Rules::SkipTaskbar);
    if (was_wants_tab_focus != wantsTabFocus()) {
        FocusChain::self()->update(this, isActive() ? FocusChain::MakeFirst : FocusChain::Update);
    }
    emit skipTaskbarChanged();
}

void AbstractClient::setOriginalSkipTaskbar(bool b)
{
    m_originalSkipTaskbar = rules()->checkSkipTaskbar(b);
    setSkipTaskbar(m_originalSkipTaskbar);
}

void AbstractClient::doSetSkipTaskbar()
{

}

void AbstractClient::setIcon(const QIcon &icon)
{
    m_icon = icon;
    emit iconChanged();
}

void AbstractClient::setActive(bool act)
{
    if (m_active == act) {
        return;
    }
    m_active = act;
    const int ruledOpacity = m_active
                             ? rules()->checkOpacityActive(qRound(opacity() * 100.0))
                             : rules()->checkOpacityInactive(qRound(opacity() * 100.0));
    setOpacity(ruledOpacity / 100.0);
    workspace()->setActiveClient(act ? this : NULL);

    if (!m_active)
        cancelAutoRaise();

    if (!m_active && shadeMode() == ShadeActivated)
        setShade(ShadeNormal);

    StackingUpdatesBlocker blocker(workspace());
    workspace()->updateClientLayer(this);   // active windows may get different layer
    auto mainclients = mainClients();
    for (auto it = mainclients.constBegin();
            it != mainclients.constEnd();
            ++it)
        if ((*it)->isFullScreen())  // fullscreens go high even if their transient is active
            workspace()->updateClientLayer(*it);

    doSetActive();
    emit activeChanged();
    updateMouseGrab();
}

void AbstractClient::doSetActive()
{
}

Layer AbstractClient::layer() const
{
    if (m_layer == UnknownLayer)
        const_cast< AbstractClient* >(this)->m_layer = belongsToLayer();
    return m_layer;
}

void AbstractClient::updateLayer()
{
    if (layer() == belongsToLayer())
        return;
    StackingUpdatesBlocker blocker(workspace());
    invalidateLayer(); // invalidate, will be updated when doing restacking
    for (auto it = transients().constBegin(),
                                  end = transients().constEnd(); it != end; ++it)
        (*it)->updateLayer();
}

void AbstractClient::invalidateLayer()
{
    m_layer = UnknownLayer;
}

Layer AbstractClient::belongsToLayer() const
{
    // NOTICE while showingDesktop, desktops move to the AboveLayer
    // (interchangeable w/ eg. yakuake etc. which will at first remain visible)
    // and the docks move into the NotificationLayer (which is between Above- and
    // ActiveLayer, so that active fullscreen windows will still cover everything)
    // Since the desktop is also activated, nothing should be in the ActiveLayer, though
    if (isDesktop())
        return workspace()->showingDesktop() ? AboveLayer : DesktopLayer;
    if (isSplash())          // no damn annoying splashscreens
        return NormalLayer; // getting in the way of everything else
    if (isDock()) {
        if (workspace()->showingDesktop())
            return NotificationLayer;
        return layerForDock();
    }
    if (isOnScreenDisplay())
        return OnScreenDisplayLayer;
    if (isNotification())
        return NotificationLayer;
    if (workspace()->showingDesktop() && belongsToDesktop()) {
        return AboveLayer;
    }
    if (keepBelow())
        return BelowLayer;
    if (isActiveFullScreen())
        return ActiveLayer;
    if (keepAbove())
        return AboveLayer;

    return NormalLayer;
}

bool AbstractClient::belongsToDesktop() const
{
    return false;
}

Layer AbstractClient::layerForDock() const
{
    // slight hack for the 'allow window to cover panel' Kicker setting
    // don't move keepbelow docks below normal window, but only to the same
    // layer, so that both may be raised to cover the other
    if (keepBelow())
        return NormalLayer;
    if (keepAbove()) // slight hack for the autohiding panels
        return AboveLayer;
    return DockLayer;
}

void AbstractClient::setKeepAbove(bool b)
{
    b = rules()->checkKeepAbove(b);
    if (b && !rules()->checkKeepBelow(false))
        setKeepBelow(false);
    if (b == keepAbove()) {
        // force hint change if different
        if (info && bool(info->state() & NET::KeepAbove) != keepAbove())
            info->setState(keepAbove() ? NET::KeepAbove : NET::States(0), NET::KeepAbove);
        return;
    }
    m_keepAbove = b;
    if (info) {
        info->setState(keepAbove() ? NET::KeepAbove : NET::States(0), NET::KeepAbove);
    }
    workspace()->updateClientLayer(this);
    updateWindowRules(Rules::Above);

    doSetKeepAbove();
    emit keepAboveChanged(m_keepAbove);
}

void AbstractClient::doSetKeepAbove()
{
}

void AbstractClient::setKeepBelow(bool b)
{
    b = rules()->checkKeepBelow(b);
    if (b && !rules()->checkKeepAbove(false))
        setKeepAbove(false);
    if (b == keepBelow()) {
        // force hint change if different
        if (info && bool(info->state() & NET::KeepBelow) != keepBelow())
            info->setState(keepBelow() ? NET::KeepBelow : NET::States(0), NET::KeepBelow);
        return;
    }
    m_keepBelow = b;
    if (info) {
        info->setState(keepBelow() ? NET::KeepBelow : NET::States(0), NET::KeepBelow);
    }
    workspace()->updateClientLayer(this);
    updateWindowRules(Rules::Below);

    doSetKeepBelow();
    emit keepBelowChanged(m_keepBelow);
}

void AbstractClient::doSetKeepBelow()
{
}

void AbstractClient::startAutoRaise()
{
    delete m_autoRaiseTimer;
    m_autoRaiseTimer = new QTimer(this);
    connect(m_autoRaiseTimer, &QTimer::timeout, this, &AbstractClient::autoRaise);
    m_autoRaiseTimer->setSingleShot(true);
    m_autoRaiseTimer->start(options->autoRaiseInterval());
}

void AbstractClient::cancelAutoRaise()
{
    delete m_autoRaiseTimer;
    m_autoRaiseTimer = nullptr;
}

void AbstractClient::autoRaise()
{
    workspace()->raiseClient(this);
    cancelAutoRaise();
}

bool AbstractClient::wantsTabFocus() const
{
    return (isNormalWindow() || isDialog()) && wantsInput();
}

bool AbstractClient::isSpecialWindow() const
{
    // TODO
    return isDesktop() || isDock() || isSplash() || isToolbar() || isNotification() || isOnScreenDisplay();
}

void AbstractClient::demandAttention(bool set)
{
    if (isActive())
        set = false;
    if (m_demandsAttention == set)
        return;
    m_demandsAttention = set;
    if (info) {
        info->setState(set ? NET::DemandsAttention : NET::States(0), NET::DemandsAttention);
    }
    workspace()->clientAttentionChanged(this, set);
    emit demandsAttentionChanged();
}

void AbstractClient::setDesktop(int desktop)
{
    const int numberOfDesktops = VirtualDesktopManager::self()->count();
    if (desktop != NET::OnAllDesktops)   // Do range check
        desktop = qMax(1, qMin(numberOfDesktops, desktop));
    desktop = qMin(numberOfDesktops, rules()->checkDesktop(desktop));
    if (m_desktop == desktop)
        return;

    int was_desk = m_desktop;
    const bool wasOnCurrentDesktop = isOnCurrentDesktop();
    m_desktop = desktop;

    if (info) {
        info->setDesktop(desktop);
    }
    if ((was_desk == NET::OnAllDesktops) != (desktop == NET::OnAllDesktops)) {
        // onAllDesktops changed
        workspace()->updateOnAllDesktopsOfTransients(this);
    }

    auto transients_stacking_order = workspace()->ensureStackingOrder(transients());
    for (auto it = transients_stacking_order.constBegin();
            it != transients_stacking_order.constEnd();
            ++it)
        (*it)->setDesktop(desktop);

    if (isModal())  // if a modal dialog is moved, move the mainwindow with it as otherwise
        // the (just moved) modal dialog will confusingly return to the mainwindow with
        // the next desktop change
    {
        foreach (AbstractClient * c2, mainClients())
        c2->setDesktop(desktop);
    }

    doSetDesktop(desktop, was_desk);

    FocusChain::self()->update(this, FocusChain::MakeFirst);
    updateWindowRules(Rules::Desktop);

    emit desktopChanged();
    if (wasOnCurrentDesktop != isOnCurrentDesktop())
        emit desktopPresenceChanged(this, was_desk);
}

void AbstractClient::doSetDesktop(int desktop, int was_desk)
{
    Q_UNUSED(desktop)
    Q_UNUSED(was_desk)
}

void AbstractClient::setOnAllDesktops(bool b)
{
    if ((b && isOnAllDesktops()) ||
            (!b && !isOnAllDesktops()))
        return;
    if (b)
        setDesktop(NET::OnAllDesktops);
    else
        setDesktop(VirtualDesktopManager::self()->current());
}

bool AbstractClient::isShadeable() const
{
    return false;
}

void AbstractClient::setShade(bool set)
{
    set ? setShade(ShadeNormal) : setShade(ShadeNone);
}

void AbstractClient::setShade(ShadeMode mode)
{
    Q_UNUSED(mode)
}

ShadeMode AbstractClient::shadeMode() const
{
    return ShadeNone;
}

AbstractClient::Position AbstractClient::titlebarPosition() const
{
    // TODO: still needed, remove?
    return PositionTop;
}

void AbstractClient::setMinimized(bool set)
{
    set ? minimize() : unminimize();
}

void AbstractClient::minimize(bool avoid_animation)
{
    if (!isMinimizable() || isMinimized())
        return;

    if (isShade() && info) // NETWM restriction - KWindowInfo::isMinimized() == Hidden && !Shaded
        info->setState(0, NET::Shaded);

    m_minimized = true;

    doMinimize();

    updateWindowRules(Rules::Minimize);
    FocusChain::self()->update(this, FocusChain::MakeFirstMinimized);
    // TODO: merge signal with s_minimized
    emit clientMinimized(this, !avoid_animation);
    emit minimizedChanged();
}

void AbstractClient::unminimize(bool avoid_animation)
{
    if (!isMinimized())
        return;

    if (rules()->checkMinimize(false)) {
        return;
    }

    if (isShade() && info) // NETWM restriction - KWindowInfo::isMinimized() == Hidden && !Shaded
        info->setState(NET::Shaded, NET::Shaded);

    m_minimized = false;

    doMinimize();

    updateWindowRules(Rules::Minimize);
    emit clientUnminimized(this, !avoid_animation);
    emit minimizedChanged();
}

void AbstractClient::doMinimize()
{
}

QPalette AbstractClient::palette() const
{
    if (!m_palette) {
        return QPalette();
    }
    return m_palette->palette();
}

const Decoration::DecorationPalette *AbstractClient::decorationPalette() const
{
    return m_palette.get();
}

void AbstractClient::updateColorScheme(QString path)
{
    if (path.isEmpty()) {
        path = QStringLiteral("kdeglobals");
    }

    if (!m_palette || m_colorScheme != path) {
        m_colorScheme = path;

        if (m_palette) {
            disconnect(m_palette.get(), &Decoration::DecorationPalette::changed, this, &AbstractClient::handlePaletteChange);
        }

        auto it = s_palettes.find(m_colorScheme);

        if (it == s_palettes.end() || it->expired()) {
            m_palette = std::make_shared<Decoration::DecorationPalette>(m_colorScheme);
            if (m_palette->isValid()) {
                s_palettes[m_colorScheme] = m_palette;
            } else {
                if (!s_defaultPalette) {
                    s_defaultPalette = std::make_shared<Decoration::DecorationPalette>(QStringLiteral("kdeglobals"));
                    s_palettes[QStringLiteral("kdeglobals")] = s_defaultPalette;
                }

                m_palette = s_defaultPalette;
            }

            if (m_colorScheme == QStringLiteral("kdeglobals")) {
                s_defaultPalette = m_palette;
            }
        } else {
            m_palette = it->lock();
        }

        connect(m_palette.get(), &Decoration::DecorationPalette::changed, this, &AbstractClient::handlePaletteChange);

        emit paletteChanged(palette());
    }
}

void AbstractClient::handlePaletteChange()
{
    emit paletteChanged(palette());
}

void AbstractClient::keepInArea(QRect area, bool partial)
{
    if (partial) {
        // increase the area so that can have only 100 pixels in the area
        area.setLeft(qMin(area.left() - width() + 100, area.left()));
        area.setTop(qMin(area.top() - height() + 100, area.top()));
        area.setRight(qMax(area.right() + width() - 100, area.right()));
        area.setBottom(qMax(area.bottom() + height() - 100, area.bottom()));
    }
    if (!partial) {
        // resize to fit into area
        if (area.width() < width() || area.height() < height())
            resizeWithChecks(qMin(area.width(), width()), qMin(area.height(), height()));
    }
    int tx = x(), ty = y();
    if (geometry().right() > area.right() && width() <= area.width())
        tx = area.right() - width() + 1;
    if (geometry().bottom() > area.bottom() && height() <= area.height())
        ty = area.bottom() - height() + 1;
    if (!area.contains(geometry().topLeft())) {
        if (tx < area.x())
            tx = area.x();
        if (ty < area.y())
            ty = area.y();
    }
    if (tx != x() || ty != y())
        move(tx, ty);
}

QSize AbstractClient::maxSize() const
{
    return rules()->checkMaxSize(QSize(INT_MAX, INT_MAX));
}

QSize AbstractClient::minSize() const
{
    return rules()->checkMinSize(QSize(0, 0));
}

void AbstractClient::updateMoveResize(const QPointF &currentGlobalCursor)
{
    Q_UNUSED(currentGlobalCursor)
}

bool AbstractClient::hasStrut() const
{
    return false;
}

void AbstractClient::setupWindowManagementInterface()
{
    if (m_windowManagementInterface) {
        // already setup
        return;
    }
    if (!waylandServer() || !surface()) {
        return;
    }
    if (!waylandServer()->windowManagement()) {
        return;
    }
    using namespace KWayland::Server;
    auto w = waylandServer()->windowManagement()->createWindow(this);
    w->setTitle(caption());
    w->setVirtualDesktop(isOnAllDesktops() ? 0 : desktop() - 1);
    w->setActive(isActive());
    w->setFullscreen(isFullScreen());
    w->setKeepAbove(keepAbove());
    w->setKeepBelow(keepBelow());
    w->setMaximized(maximizeMode() == KWin::MaximizeFull);
    w->setMinimized(isMinimized());
    w->setOnAllDesktops(isOnAllDesktops());
    w->setDemandsAttention(isDemandingAttention());
    w->setCloseable(isCloseable());
    w->setMaximizeable(isMaximizable());
    w->setMinimizeable(isMinimizable());
    w->setFullscreenable(isFullScreenable());
    w->setThemedIconName(icon().name().isEmpty() ? QStringLiteral("xorg") : icon().name());
    w->setAppId(QString::fromUtf8(resourceName()));
    w->setSkipTaskbar(skipTaskbar());
    connect(this, &AbstractClient::skipTaskbarChanged, w,
        [w, this] {
            w->setSkipTaskbar(skipTaskbar());
        }
    );
    connect(this, &AbstractClient::captionChanged, w, [w, this] { w->setTitle(caption()); });
    connect(this, &AbstractClient::desktopChanged, w,
        [w, this] {
            if (isOnAllDesktops()) {
                w->setOnAllDesktops(true);
                return;
            }
            w->setVirtualDesktop(desktop() - 1);
            w->setOnAllDesktops(false);
        }
    );
    connect(this, &AbstractClient::activeChanged, w, [w, this] { w->setActive(isActive()); });
    connect(this, &AbstractClient::fullScreenChanged, w, [w, this] { w->setFullscreen(isFullScreen()); });
    connect(this, &AbstractClient::keepAboveChanged, w, &PlasmaWindowInterface::setKeepAbove);
    connect(this, &AbstractClient::keepBelowChanged, w, &PlasmaWindowInterface::setKeepBelow);
    connect(this, &AbstractClient::minimizedChanged, w, [w, this] { w->setMinimized(isMinimized()); });
    connect(this, static_cast<void (AbstractClient::*)(AbstractClient*,MaximizeMode)>(&AbstractClient::clientMaximizedStateChanged), w,
        [w] (KWin::AbstractClient *c, MaximizeMode mode) {
            Q_UNUSED(c);
            w->setMaximized(mode == KWin::MaximizeFull);
        }
    );
    connect(this, &AbstractClient::demandsAttentionChanged, w, [w, this] { w->setDemandsAttention(isDemandingAttention()); });
    connect(this, &AbstractClient::iconChanged, w,
        [w, this] {
            const QIcon i = icon();
            w->setThemedIconName(i.name().isEmpty() ? QStringLiteral("xorg") : i.name());
        }
    );
    connect(this, &AbstractClient::windowClassChanged, w,
        [w, this] {
            w->setAppId(QString::fromUtf8(resourceName()));
        }
    );
    connect(w, &PlasmaWindowInterface::closeRequested, this, [this] { closeWindow(); });
    connect(w, &PlasmaWindowInterface::virtualDesktopRequested, this,
        [this] (quint32 desktop) {
            workspace()->sendClientToDesktop(this, desktop + 1, true);
        }
    );
    connect(w, &PlasmaWindowInterface::fullscreenRequested, this,
        [this] (bool set) {
            setFullScreen(set, false);
        }
    );
    connect(w, &PlasmaWindowInterface::minimizedRequested, this,
        [this] (bool set) {
            if (set) {
                minimize();
            } else {
                unminimize();
            }
        }
    );
    connect(w, &PlasmaWindowInterface::maximizedRequested, this,
        [this] (bool set) {
            maximize(set ? MaximizeFull : MaximizeRestore);
        }
    );
    connect(w, &PlasmaWindowInterface::keepAboveRequested, this,
        [this] (bool set) {
            setKeepAbove(set);
        }
    );
    connect(w, &PlasmaWindowInterface::keepBelowRequested, this,
        [this] (bool set) {
            setKeepBelow(set);
        }
    );
    connect(w, &PlasmaWindowInterface::demandsAttentionRequested, this,
        [this] (bool set) {
            demandAttention(set);
        }
    );
    connect(w, &PlasmaWindowInterface::activeRequested, this,
        [this] (bool set) {
            if (set) {
                workspace()->activateClient(this, true);
            }
        }
    );
    m_windowManagementInterface = w;
}

void AbstractClient::destroyWindowManagementInterface()
{
    delete m_windowManagementInterface;
    m_windowManagementInterface = nullptr;
}

Options::MouseCommand AbstractClient::getMouseCommand(Qt::MouseButton button, bool *handled) const
{
    *handled = false;
    if (button == Qt::NoButton) {
        return Options::MouseNothing;
    }
    if (isActive()) {
        if (options->isClickRaise()) {
            *handled = true;
            return Options::MouseActivateRaiseAndPassClick;
        }
    } else {
        *handled = true;
        switch (button) {
        case Qt::LeftButton:
            return options->commandWindow1();
        case Qt::MiddleButton:
            return options->commandWindow2();
        case Qt::RightButton:
            return options->commandWindow3();
        default:
            // all other buttons pass Activate & Pass Client
            return Options::MouseActivateAndPassClick;
        }
    }
    return Options::MouseNothing;
}

Options::MouseCommand AbstractClient::getWheelCommand(Qt::Orientation orientation, bool *handled) const
{
    *handled = false;
    if (orientation != Qt::Vertical) {
        return Options::MouseNothing;
    }
    if (!isActive()) {
        *handled = true;
        return options->commandWindowWheel();
    }
    return Options::MouseNothing;
}

bool AbstractClient::performMouseCommand(Options::MouseCommand cmd, const QPoint &globalPos)
{
    bool replay = false;
    switch(cmd) {
    case Options::MouseRaise:
        workspace()->raiseClient(this);
        break;
    case Options::MouseLower: {
        workspace()->lowerClient(this);
        break;
    }
    case Options::MouseOperationsMenu:
        if (isActive() && options->isClickRaise())
            autoRaise();
        workspace()->showWindowMenu(QRect(globalPos, globalPos), this);
        break;
    case Options::MouseToggleRaiseAndLower:
        workspace()->raiseOrLowerClient(this);
        break;
    case Options::MouseActivateAndRaise: {
        replay = isActive(); // for clickraise mode
        bool mustReplay = !rules()->checkAcceptFocus(info->input());
        if (mustReplay) {
            ToplevelList::const_iterator  it = workspace()->stackingOrder().constEnd(),
                                     begin = workspace()->stackingOrder().constBegin();
            while (mustReplay && --it != begin && *it != this) {
                AbstractClient *c = qobject_cast<AbstractClient*>(*it);
                if (!c || (c->keepAbove() && !keepAbove()) || (keepBelow() && !c->keepBelow()))
                    continue; // can never raise above "it"
                mustReplay = !(c->isOnCurrentDesktop() && c->isOnCurrentActivity() && c->geometry().intersects(geometry()));
            }
        }
        workspace()->takeActivity(this, Workspace::ActivityFocus | Workspace::ActivityRaise);
        screens()->setCurrent(globalPos);
        replay = replay || mustReplay;
        break;
    }
    case Options::MouseActivateAndLower:
        workspace()->requestFocus(this);
        workspace()->lowerClient(this);
        screens()->setCurrent(globalPos);
        replay = replay || !rules()->checkAcceptFocus(info->input());
        break;
    case Options::MouseActivate:
        replay = isActive(); // for clickraise mode
        workspace()->takeActivity(this, Workspace::ActivityFocus);
        screens()->setCurrent(globalPos);
        replay = replay || !rules()->checkAcceptFocus(info->input());
        break;
    case Options::MouseActivateRaiseAndPassClick:
        workspace()->takeActivity(this, Workspace::ActivityFocus | Workspace::ActivityRaise);
        screens()->setCurrent(globalPos);
        replay = true;
        break;
    case Options::MouseActivateAndPassClick:
        workspace()->takeActivity(this, Workspace::ActivityFocus);
        screens()->setCurrent(globalPos);
        replay = true;
        break;
    case Options::MouseMaximize:
        maximize(MaximizeFull);
        break;
    case Options::MouseRestore:
        maximize(MaximizeRestore);
        break;
    case Options::MouseMinimize:
        minimize();
        break;
    case Options::MouseAbove: {
        StackingUpdatesBlocker blocker(workspace());
        if (keepBelow())
            setKeepBelow(false);
        else
            setKeepAbove(true);
        break;
    }
    case Options::MouseBelow: {
        StackingUpdatesBlocker blocker(workspace());
        if (keepAbove())
            setKeepAbove(false);
        else
            setKeepBelow(true);
        break;
    }
    case Options::MousePreviousDesktop:
        workspace()->windowToPreviousDesktop(this);
        break;
    case Options::MouseNextDesktop:
        workspace()->windowToNextDesktop(this);
        break;
    case Options::MouseOpacityMore:
        if (!isDesktop())   // No point in changing the opacity of the desktop
            setOpacity(qMin(opacity() + 0.1, 1.0));
        break;
    case Options::MouseOpacityLess:
        if (!isDesktop())   // No point in changing the opacity of the desktop
            setOpacity(qMax(opacity() - 0.1, 0.1));
        break;
    case Options::MousePreviousTab:
        if (tabGroup())
            tabGroup()->activatePrev();
    break;
    case Options::MouseNextTab:
        if (tabGroup())
            tabGroup()->activateNext();
    break;
    case Options::MouseClose:
        closeWindow();
        break;
    case Options::MouseDragTab:
    case Options::MouseNothing:
    default:
        replay = true;
        break;
    }
    return replay;
}

void AbstractClient::setTransientFor(AbstractClient *transientFor)
{
    if (transientFor == this) {
        // cannot be transient for one self
        return;
    }
    if (m_transientFor == transientFor) {
        return;
    }
    m_transientFor = transientFor;
    emit transientChanged();
}

const AbstractClient *AbstractClient::transientFor() const
{
    return m_transientFor;
}

AbstractClient *AbstractClient::transientFor()
{
    return m_transientFor;
}

bool AbstractClient::hasTransientPlacementHint() const
{
    return false;
}

QPoint AbstractClient::transientPlacementHint() const
{
    return QPoint();
}

bool AbstractClient::hasTransient(const AbstractClient *c, bool indirect) const
{
    Q_UNUSED(indirect);
    return c->transientFor() == this;
}

QList< AbstractClient* > AbstractClient::mainClients() const
{
    if (const AbstractClient *t = transientFor()) {
        return QList<AbstractClient*>{const_cast< AbstractClient* >(t)};
    }
    return QList<AbstractClient*>();
}

QList<AbstractClient*> AbstractClient::allMainClients() const
{
    auto result = mainClients();
    foreach (const auto *cl, result) {
        result += cl->allMainClients();
    }
    return result;
}

void AbstractClient::setModal(bool m)
{
    // Qt-3.2 can have even modal normal windows :(
    if (m_modal == m)
        return;
    m_modal = m;
    emit modalChanged();
    // Changing modality for a mapped window is weird (?)
    // _NET_WM_STATE_MODAL should possibly rather be _NET_WM_WINDOW_TYPE_MODAL_DIALOG
}

bool AbstractClient::isModal() const
{
    return m_modal;
}

void AbstractClient::addTransient(AbstractClient *cl)
{
    assert(!m_transients.contains(cl));
    assert(cl != this);
    m_transients.append(cl);
}

void AbstractClient::removeTransient(AbstractClient *cl)
{
    m_transients.removeAll(cl);
    if (cl->transientFor() == this) {
        cl->setTransientFor(nullptr);
    }
}

void AbstractClient::removeTransientFromList(AbstractClient *cl)
{
    m_transients.removeAll(cl);
}

bool AbstractClient::isActiveFullScreen() const
{
    if (!isFullScreen())
        return false;

    const auto ac = workspace()->mostRecentlyActivatedClient(); // instead of activeClient() - avoids flicker
    // according to NETWM spec implementation notes suggests
    // "focused windows having state _NET_WM_STATE_FULLSCREEN" to be on the highest layer.
    // we'll also take the screen into account
    return ac && (ac == this || ac->screen() != screen());
}

int AbstractClient::borderBottom() const
{
    return 0;
}

int AbstractClient::borderLeft() const
{
    return 0;
}

int AbstractClient::borderRight() const
{
    return 0;
}

int AbstractClient::borderTop() const
{
    return 0;
}

QSize AbstractClient::sizeForClientSize(const QSize &wsize, Sizemode mode, bool noframe) const
{
    Q_UNUSED(mode)
    Q_UNUSED(noframe)
    return wsize;
}

bool AbstractClient::isDecorated() const
{
    return false;
}

void AbstractClient::addRepaintDuringGeometryUpdates()
{
    const QRect deco_rect = visibleRect();
    addLayerRepaint(m_visibleRectBeforeGeometryUpdate);
    addLayerRepaint(deco_rect);   // trigger repaint of window's new location
    m_visibleRectBeforeGeometryUpdate = deco_rect;
}

void AbstractClient::updateGeometryBeforeUpdateBlocking()
{
    m_geometryBeforeUpdateBlocking = geom;
}

void AbstractClient::updateTabGroupStates(TabGroup::States)
{
}

void AbstractClient::doMove(int, int)
{
}

void AbstractClient::updateInitialMoveResizeGeometry()
{
    m_moveResize.initialGeometry = geometry();
    m_moveResize.geometry = m_moveResize.initialGeometry;
    m_moveResize.startScreen = screen();
}

void AbstractClient::updateCursor()
{
    Position m = moveResizePointerMode();
    if (!isResizable() || isShade())
        m = PositionCenter;
    Qt::CursorShape c = Qt::ArrowCursor;
    switch(m) {
    case PositionTopLeft:
    case PositionBottomRight:
        c = Qt::SizeFDiagCursor;
        break;
    case PositionBottomLeft:
    case PositionTopRight:
        c = Qt::SizeBDiagCursor;
        break;
    case PositionTop:
    case PositionBottom:
        c = Qt::SizeVerCursor;
        break;
    case PositionLeft:
    case PositionRight:
        c = Qt::SizeHorCursor;
        break;
    default:
        if (isMoveResize())
            c = Qt::SizeAllCursor;
        else
            c = Qt::ArrowCursor;
        break;
    }
    if (c == m_moveResize.cursor)
        return;
    m_moveResize.cursor = c;
    emit moveResizeCursorChanged(c);
}

void AbstractClient::leaveMoveResize()
{
    workspace()->setClientIsMoving(nullptr);
    setMoveResize(false);
    if (ScreenEdges::self()->isDesktopSwitchingMovingClients())
        ScreenEdges::self()->reserveDesktopSwitching(false, Qt::Vertical|Qt::Horizontal);
    if (isElectricBorderMaximizing()) {
        outline()->hide();
        elevate(false);
    }
}

bool AbstractClient::s_haveResizeEffect = false;

void AbstractClient::updateHaveResizeEffect()
{
    s_haveResizeEffect = effects && static_cast<EffectsHandlerImpl*>(effects)->provides(Effect::Resize);
}

bool AbstractClient::doStartMoveResize()
{
    return true;
}

}

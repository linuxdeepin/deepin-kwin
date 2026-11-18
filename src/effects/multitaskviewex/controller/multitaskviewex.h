/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "controller/dockrelay.h"
#include "controller/effectconfig.h"
#include "controller/wallpapermanager.h"
#include "controller/windowroles.h"
#include "model/elevationstate.h"
#include "view/glscene.h"
#include "view/screenview.h"
#include "view/screenviewset.h"

#include <kwineffects.h>

#include <QElapsedTimer>
#include <QHash>
#include <QKeySequence>
#include <QList>
#include <QTimer>

#include <chrono>
#include <memory>

namespace KWin
{

class Window;
class ImageProvider;
class WorkspaceManager;
class WorkspaceModel;

/**
 * @brief Multi-task view effect: a full-screen grid of windows and workspaces.
 *
 * The effect owns the painting pipeline and the input dispatch. Desktop facts
 * live in WorkspaceManager, wallpaper lookup in WallpaperManager and the effect
 * settings in EffectConfig.
 */
class MultitaskViewExEffect : public Effect
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the multi-task view effect.
     */
    MultitaskViewExEffect();

    /**
     * @brief Destroys the effect.
     */
    ~MultitaskViewExEffect() override;

    /**
     * @brief Tests whether the effect is supported on the current platform.
     * @return True when supported.
     */
    static bool supported();

    /**
     * @brief Tests whether the effect is enabled by default.
     * @return True when enabled by default.
     */
    static bool enabledByDefault();

    /**
     * @brief Applies configuration changes.
     * @param flags Reconfigure flags.
     */
    void reconfigure(ReconfigureFlags flags) override;

    /**
     * @brief Advances the effect and prepares a screen for painting.
     * @param data Screen pre-paint data.
     * @param presentTime Frame presentation time.
     */
    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;

    /**
     * @brief Paints the screen.
     * @param mask Paint mask.
     * @param region Damaged region.
     * @param data Screen paint data.
     */
    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;

    /**
     * @brief Finalizes painting and requests the next frame if needed.
     */
    void postPaintScreen() override;

    /**
     * @brief Handles a grabbed keyboard event.
     * @param keyEvent Key event to process.
     */
    void grabbedKeyboardEvent(QKeyEvent *keyEvent) override;

    /**
     * @brief Returns the requested position in the effect chain.
     * @return The effect chain position.
     */
    int requestedEffectChainPosition() const override;

    /**
     * @brief Handles activation through a screen edge.
     * @param border Triggered electric border.
     * @return True when the border activation was handled.
     */
    bool borderActivated(ElectricBorder border) override;

    /**
     * @brief Paints a window through the effect chain.
     * @param w Window to paint.
     * @param mask Paint mask.
     * @param region Damaged region.
     * @param data Window paint data.
     */
    void drawWindow(EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data) override;

public Q_SLOTS:
    /**
     * @brief Toggles the multi-task view on or off.
     */
    Q_SCRIPTABLE void toggle();

    /**
     * @brief Tests whether the multi-task view is active.
     * @return True when the view is active.
     */
    Q_SCRIPTABLE bool viewActive() const;

    /**
     * @brief Requests the multi-task view to stop.
     */
    Q_SCRIPTABLE void stop();

    /**
     * @brief Activates the multi-task view.
     */
    void activate();

Q_SIGNALS:
    /**
     * @brief Emitted when the running state of the view changes.
     */
    void stateChanged();

private:
    WorkspaceModel *workspaces() const;
    void setRunning(bool run);
    void requestExit();
    void elevateForExit();
    void clearElevatedWindow();

    void syncViews();
    void wireGlobalTimers();
    void wireScreenView(ScreenView &view);
    ScreenView *ensureScreenView(EffectScreen *screen);
    void applyViewGeometry(ScreenView &view);
    QVector<ScreenView *> paintTargetViews(const QRectF &renderTargetRect) const;
    ScreenView *viewAtGlobal(const QPointF &globalPos) const;
    ScreenView *viewWithActivePress() const;
    bool inDockArea(const QPointF &pos) const;
    void paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data) override;
    void windowInputMouseEvent(QEvent *event) override;
    ScreenView *dragView() const
    {
        return m_dragView;
    }
    ScreenView *m_dragView = nullptr;

    void acquireBackground(const QString &uri);
    void acquireBackground(ScreenView &view, const QString &uri);
    void releaseBackground(ScreenView &view);
    void syncThumbPins();
    QHash<QString, QPair<QString, QSize>> m_thumbPins;

    void finishBarDragIfButtonReleased();
    void finishBarDrag();
    void cancelBarDrag(ScreenView &view);
    QVector<int> barOrder() const;
    void syncBarModel();
    void applyBarOrder(ScreenView &view);

    void onWallpapersRefreshed();
    void prewarmTextures();
    void reapplyBackgrounds();

    void refreshModel();
    void addDesktop();
    void removeDesktop(int desktopNumber);
    void onVirtualDesktopsChanged();
    void warmWallpapers();
    void restoreWindowRoles();

    void syncSessionItems();
    void syncScreenItems(ScreenView &view, const QVector<EffectWindow *> &wins,
                         const QVector<QRectF> &absDocks);
    void setupSession();
    void teardownSession();
    void refreshOsScale();
    void slideToDesktop(int num, int fromOverride = -1);
    void slideByWheel(int wheelY);
    void gridScrollBy(ScreenView &view, qreal angleDeltaY);

    void updateEmptyHint(ScreenView &view);
    void updateWindowInfoHint(ScreenView &view);

    EffectWindow *hoverWindow(ScreenView &view) const;
    void setHoverWindow(ScreenView &view, EffectWindow *w);
    EffectWindow *stepWindow(ScreenView &view, EffectWindow *w, int step) const;
    EffectWindow *firstOrLastWindow(ScreenView &view, bool last) const;
    EffectWindow *overlayWindowAt(ScreenView &view, int index) const;
    EffectWindow *sameTypeWindow(ScreenView &view, EffectWindow *w, int step) const;
    bool cycleHoverWindow(ScreenView &view, int step);
    void moveHoverToDesktopIfAvailable(ScreenView &view, int num);
    void moveHoverWindowToDesktop(int num);

    void onWindowClosed(EffectWindow *w);
    void onWindowDeleted(EffectWindow *w);
    void onWindowAdded(EffectWindow *w);
    static bool isLockScreenWindow(EffectWindow *w);
    bool sessionLocked() const;
    void relayDockClick();
    void applyConfigToViews();

private:
    struct ClosingWindow
    {
        EffectWindow *window = nullptr;
        EffectWindowDeletedRef deletedRef;
        EffectWindowVisibleRef visibleRef;
    };
    QHash<EffectWindow *, ClosingWindow> m_closing;
    QHash<EffectWindow *, EffectWindowVisibleRef> m_visibleRefs;
    QHash<EffectWindow *, EffectWindowVisibleRef> m_gridMinimizeRefs;

    WorkspaceManager *m_workspaces = nullptr;
    WallpaperManager *m_wallpapers = nullptr;
    EffectConfig *m_config = nullptr;
    ImageProvider *m_imgProvider = nullptr;
    WindowRoles m_roles;
    DockRelay m_dockRelay;
    Qt::MouseButton m_dockPressButton = Qt::NoButton;
    ScreenView *m_dockPressView = nullptr;

    ScreenViewSet m_views;
    QString m_newWorkspaceWallpaper;
    bool m_pendingCommitPreview = false;
    bool m_addHovered = false;
    bool m_pendingPreviewShow = false;
    qreal m_osScale = 1.0;

    bool m_running = false;
    bool m_hasKeyboardGrab = false;
    bool m_trimPending = false;
    bool m_teardownPending = false;
    bool m_consumeBackgroundPass = false;
    bool m_exitRequested = false;
    bool m_inSlideTransition = false;
    EffectWindow *m_selectedWindow = nullptr;
    ExitElevationState m_elevation;
    int m_sessionDesktop = 0;

    QAction *m_toggleAction = nullptr;
    QList<QKeySequence> m_toggleShortcut;
    QList<ElectricBorder> m_borderActivate;

    QTimer m_hoverSwitchTimer;
    int m_hoverSwitchSlot = -1;
    QElapsedTimer m_addDesktopCooldown;
};

}

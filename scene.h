/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

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

#ifndef KWIN_SCENE_H
#define KWIN_SCENE_H

#include "toplevel.h"
#include "utils.h"
#include "kwineffects.h"

#include <QElapsedTimer>
#include <QMatrix4x4>

class QOpenGLFramebufferObject;

namespace KWayland
{
namespace Server
{
class BufferInterface;
class SubSurfaceInterface;
}
}

namespace KWin
{

namespace Decoration
{
class DecoratedClientImpl;
class Renderer;
}

class AbstractThumbnailItem;
class Deleted;
class EffectFrameImpl;
class EffectWindowImpl;
class OverlayWindow;
class Shadow;
class WindowPixmap;

// The base class for compositing backends.
class KWIN_EXPORT Scene : public QObject
{
    Q_OBJECT
public:
    explicit Scene(QObject *parent = nullptr);
    virtual ~Scene() = 0;
    class EffectFrame;
    class Window;

    // Returns true if the ctor failed to properly initialize.
    virtual bool initFailed() const = 0;
    virtual CompositingType compositingType() const = 0;

    virtual bool hasPendingFlush() const { return false; }

    // Repaints the given screen areas, windows provides the stacking order.
    // The entry point for the main part of the painting pass.
    // returns the time since the last vblank signal - if there's one
    // ie. "what of this frame is lost to painting"
    virtual qint64 paint(QRegion damage, ToplevelList windows) = 0;

    // Notification function - KWin core informs about changes.
    // Used to mainly discard cached data.

    // a new window has been created
    void windowAdded(Toplevel*);
    /**
     * @brief Creates the Scene backend of an EffectFrame.
     *
     * @param frame The EffectFrame this Scene::EffectFrame belongs to.
     */
    virtual Scene::EffectFrame *createEffectFrame(EffectFrameImpl *frame) = 0;
    /**
     * @brief Creates the Scene specific Shadow subclass.
     *
     * An implementing class has to create a proper instance. It is not allowed to
     * return @c null.
     *
     * @param toplevel The Toplevel for which the Shadow needs to be created.
     */
    virtual Shadow *createShadow(Toplevel *toplevel) = 0;
    /**
     * Method invoked when the screen geometry is changed.
     * Reimplementing classes should also invoke the parent method
     * as it takes care of resizing the overlay window.
     * @param size The new screen geometry size
     **/
    virtual void screenGeometryChanged(const QSize &size);
    // Flags controlling how painting is done.
    enum {
        // Window (or at least part of it) will be painted opaque.
        PAINT_WINDOW_OPAQUE         = 1 << 0,
        // Window (or at least part of it) will be painted translucent.
        PAINT_WINDOW_TRANSLUCENT    = 1 << 1,
        // Window will be painted with transformed geometry.
        PAINT_WINDOW_TRANSFORMED    = 1 << 2,
        // Paint only a region of the screen (can be optimized, cannot
        // be used together with TRANSFORMED flags).
        PAINT_SCREEN_REGION         = 1 << 3,
        // Whole screen will be painted with transformed geometry.
        PAINT_SCREEN_TRANSFORMED    = 1 << 4,
        // At least one window will be painted with transformed geometry.
        PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS = 1 << 5,
        // Clear whole background as the very first step, without optimizing it
        PAINT_SCREEN_BACKGROUND_FIRST = 1 << 6,
        // PAINT_DECORATION_ONLY = 1 << 7 has been removed
        // Window will be painted with a lanczos filter.
        PAINT_WINDOW_LANCZOS = 1 << 8
        // PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_WITHOUT_FULL_REPAINTS = 1 << 9 has been removed
    };
    // types of filtering available
    enum ImageFilterType { ImageFilterFast, ImageFilterGood };
    // there's nothing to paint (adjust time_diff later)
    virtual void idle();
    virtual bool blocksForRetrace() const;
    virtual bool syncsToVBlank() const;
    virtual OverlayWindow* overlayWindow() = 0;

    virtual bool makeOpenGLContextCurrent();
    virtual void doneOpenGLContextCurrent();

    virtual QMatrix4x4 screenProjectionMatrix() const;

    /**
     * Whether the Scene uses an X11 overlay window to perform compositing.
     */
    virtual bool usesOverlayWindow() const = 0;

    virtual void triggerFence();

    virtual Decoration::Renderer *createDecorationRenderer(Decoration::DecoratedClientImpl *) = 0;

    /**
     * Whether the Scene is able to drive animations.
     * This is used as a hint to the effects system which effects can be supported.
     * If the Scene performs software rendering it is supposed to return @c false,
     * if rendering is hardware accelerated it should return @c true.
     **/
    virtual bool animationsSupported() const = 0;

    /**
     * The render buffer used by an XRender based compositor scene.
     * Default implementation returns XCB_RENDER_PICTURE_NONE
     **/
    virtual xcb_render_picture_t xrenderBufferPicture() const;

    /**
     * The QPainter used by a QPainter based compositor scene.
     * Default implementation returns @c nullptr;
     **/
    virtual QPainter *scenePainter() const;

    /**
     * The render buffer used by a QPainter based compositor.
     * Default implementation returns @c nullptr.
     **/
    virtual QImage *qpainterRenderBuffer() const;

    /**
     * The backend specific extensions (e.g. EGL/GLX extensions).
     *
     * Not the OpenGL (ES) extension!
     *
     * Default implementation returns empty list
     **/
    virtual QVector<QByteArray> openGLPlatformInterfaceExtensions() const;

    virtual void setDamageRegion(QRegion region);

Q_SIGNALS:
    void frameRendered();
    void resetCompositing();

public Q_SLOTS:
    // a window has been destroyed
    void windowDeleted(KWin::Deleted*);
    // shape/size of a window changed
    void windowGeometryShapeChanged(KWin::Toplevel* c);
    // a window has been closed
    void windowClosed(KWin::Toplevel* c, KWin::Deleted* deleted);
protected:
    virtual Window *createWindow(Toplevel *toplevel) = 0;
    void createStackingOrder(ToplevelList toplevels);
    void clearStackingOrder();
    // shared implementation, starts painting the screen
    void paintScreen(int *mask, const QRegion &damage, const QRegion &repaint,
                     QRegion *updateRegion, QRegion *validRegion, const QMatrix4x4 &projection = QMatrix4x4(), const QRect &outputGeometry = QRect());
    // Render cursor texture in case hardware cursor is disabled/non-applicable
    virtual void paintCursor() = 0;
    friend class EffectsHandlerImpl;
    // called after all effects had their paintScreen() called
    void finalPaintScreen(int mask, QRegion region, ScreenPaintData& data);
    // shared implementation of painting the screen in the generic
    // (unoptimized) way
    virtual void paintGenericScreen(int mask, ScreenPaintData data);
    // shared implementation of painting the screen in an optimized way
    virtual void paintSimpleScreen(int mask, QRegion region);
    // paint the background (not the desktop background - the whole background)
    virtual void paintBackground(QRegion region) = 0;
    // called after all effects had their paintWindow() called
    void finalPaintWindow(EffectWindowImpl* w, int mask, QRegion region, WindowPaintData& data);
    // shared implementation, starts painting the window
    virtual void paintWindow(Window* w, int mask, QRegion region, WindowQuadList quads);
    // called after all effects had their drawWindow() called
    virtual void finalDrawWindow(EffectWindowImpl* w, int mask, QRegion region, WindowPaintData& data);
    // let the scene decide whether it's better to paint more of the screen, eg. in order to allow a buffer swap
    // the default is NOOP
    virtual void extendPaintRegion(QRegion &region, bool opaqueFullscreen);
    virtual void paintDesktop(int desktop, int mask, const QRegion &region, ScreenPaintData &data);
    // compute time since the last repaint
    void updateTimeDiff();
    // saved data for 2nd pass of optimized screen painting
    struct Phase2Data {
        Window *window = nullptr;
        QRegion region;
        QRegion clip;
        int mask = 0;
        WindowQuadList quads;
    };
    // The region which actually has been painted by paintScreen() and should be
    // copied from the buffer to the screen. I.e. the region returned from Scene::paintScreen().
    // Since prePaintWindow() can extend areas to paint, these changes would have to propagate
    // up all the way from paintSimpleScreen() up to paintScreen(), so save them here rather
    // than propagate them up in arguments.
    QRegion painted_region;
    // Additional damage that needs to be repaired to bring a reused back buffer up to date
    QRegion repaint_region;
    // The dirty region before it was unioned with repaint_region
    QRegion damaged_region;
    // time since last repaint
    int time_diff;
    QElapsedTimer last_time;
private:
    void paintWindowThumbnails(Scene::Window *w, QRegion region, qreal opacity, qreal brightness, qreal saturation);
    void paintDesktopThumbnails(Scene::Window *w);
    QHash< Toplevel*, Window* > m_windows;
    // windows in their stacking order
    QVector< Window* > stacking_order;
};

/**
 * Factory class to create a Scene. Needs to be implemented by the plugins.
 **/
class KWIN_EXPORT SceneFactory : public QObject
{
    Q_OBJECT
public:
    virtual ~SceneFactory();

    /**
     * @returns The created Scene, may be @c nullptr.
     **/
    virtual Scene *create(QObject *parent = nullptr) const = 0;

protected:
    explicit SceneFactory(QObject *parent);
};

// The base class for windows representations in composite backends
class Scene::Window
{
public:
    Window(Toplevel* c);
    virtual ~Window();
    // perform the actual painting of the window
    virtual void performPaint(int mask, QRegion region, WindowPaintData data) = 0;
    // do any cleanup needed when the window's composite pixmap is discarded
    void pixmapDiscarded();
    int x() const;
    int y() const;
    int width() const;
    int height() const;
    QRect geometry() const;
    QPoint pos() const;
    QSize size() const;
    QRect rect() const;
    // access to the internal window class
    // TODO eventually get rid of this
    Toplevel* window() const;
    // should the window be painted
    bool isPaintingEnabled() const;
    void resetPaintingEnabled();
    // Flags explaining why painting should be disabled
    enum {
        // Window will not be painted
        PAINT_DISABLED                 = 1 << 0,
        // Window will not be painted because it is deleted
        PAINT_DISABLED_BY_DELETE       = 1 << 1,
        // Window will not be painted because of which desktop it's on
        PAINT_DISABLED_BY_DESKTOP      = 1 << 2,
        // Window will not be painted because it is minimized
        PAINT_DISABLED_BY_MINIMIZE     = 1 << 3,
        // Window will not be painted because it is not the active window in a client group
        PAINT_DISABLED_BY_TAB_GROUP = 1 << 4,
        // Window will not be painted because it's not on the current activity
        PAINT_DISABLED_BY_ACTIVITY     = 1 << 5
    };
    void enablePainting(int reason);
    void disablePainting(int reason);
    // is the window visible at all
    bool isVisible() const;
    // is the window fully opaque
    bool isOpaque() const;
    // shape of the window
    const QRegion &shape() const;
    QRegion clientShape() const;
    void discardShape();
    void updateToplevel(Toplevel* c);
    // creates initial quad list for the window
    virtual WindowQuadList buildQuads(bool force = false) const;
    void updateShadow(Shadow* shadow);
    const Shadow* shadow() const;
    Shadow* shadow();
    void referencePreviousPixmap();
    void unreferencePreviousPixmap();
    void invalidateQuadsCache();
protected:
    WindowQuadList makeQuads(WindowQuadType type, const QRegion& reg, const QPoint &textureOffset = QPoint(0, 0), qreal textureScale = 1.0) const;
    WindowQuadList makeDecorationQuads(const QRect *rects, const QRegion &region, qreal textureScale = 1.0) const;
    /**
     * @brief Returns the WindowPixmap for this Window.
     *
     * If the WindowPixmap does not yet exist, this method will invoke createWindowPixmap.
     * If the WindowPixmap is not valid it tries to create it, in case this succeeds the WindowPixmap is
     * returned. In case it fails, the previous (and still valid) WindowPixmap is returned.
     *
     * @note This method can return @c NULL as there might neither be a valid previous nor current WindowPixmap
     * around.
     *
     * The WindowPixmap gets casted to the type passed in as a template parameter. That way this class does not
     * need to know the actual WindowPixmap subclass used by the concrete Scene implementations.
     *
     * @return The WindowPixmap casted to T* or @c NULL if there is no valid window pixmap.
     */
    template<typename T> T *windowPixmap();
    template<typename T> T *previousWindowPixmap();
    /**
     * @brief Factory method to create a WindowPixmap.
     *
     * The inheriting classes need to implement this method to create a new instance of their WindowPixmap subclass.
     * @note Do not use WindowPixmap::create on the created instance. The Scene will take care of that.
     */
    virtual WindowPixmap *createWindowPixmap() = 0;
    Toplevel* toplevel;
    ImageFilterType filter;
    Shadow *m_shadow;
private:
    QScopedPointer<WindowPixmap> m_currentPixmap;
    QScopedPointer<WindowPixmap> m_previousPixmap;
    int m_referencePixmapCounter;
    int disable_painting;
    mutable QRegion shape_region;
    mutable bool shape_valid;
    mutable QScopedPointer<WindowQuadList> cached_quad_list;
    Q_DISABLE_COPY(Window)
};

/**
 * @brief Wrapper for a pixmap of the Scene::Window.
 *
 * This class encapsulates the functionality to get the pixmap for a window. When initialized the pixmap is not yet
 * mapped to the window and isValid will return @c false. The pixmap mapping to the window can be established
 * through @ref create. If it succeeds isValid will return @c true, otherwise it will keep in the non valid
 * state and it can be tried to create the pixmap mapping again (e.g. in the next frame).
 *
 * This class is not intended to be updated when the pixmap is no longer valid due to e.g. resizing the window.
 * Instead a new instance of this class should be instantiated. The idea behind this is that a valid pixmap does not
 * get destroyed, but can continue to be used. To indicate that a newer pixmap should in generally be around, one can
 * use markAsDiscarded.
 *
 * This class is intended to be inherited for the needs of the compositor backends which need further mapping from
 * the native pixmap to the respective rendering format.
 */
class KWIN_EXPORT WindowPixmap
{
public:
    virtual ~WindowPixmap();
    /**
     * @brief Tries to create the mapping between the Window and the pixmap.
     *
     * In case this method succeeds in creating the pixmap for the window, isValid will return @c true otherwise
     * @c false.
     *
     * Inheriting classes should re-implement this method in case they need to add further functionality for mapping the
     * native pixmap to the rendering format.
     */
    virtual void create();
    /**
     * @return @c true if the pixmap has been created and is valid, @c false otherwise
     */
    virtual bool isValid() const;
    /**
     * @return The native X11 pixmap handle
     */
    xcb_pixmap_t pixmap() const;
    /**
     * @return The Wayland BufferInterface for this WindowPixmap.
     **/
    QPointer<KWayland::Server::BufferInterface> buffer() const;
    const QSharedPointer<QOpenGLFramebufferObject> &fbo() const;
    /**
     * @brief Whether this WindowPixmap is considered as discarded. This means the window has changed in a way that a new
     * WindowPixmap should have been created already.
     *
     * @return @c true if this WindowPixmap is considered as discarded, @c false otherwise.
     * @see markAsDiscarded
     */
    bool isDiscarded() const;
    /**
     * @brief Marks this WindowPixmap as discarded. From now on isDiscarded will return @c true. This method should
     * only be used by the Window when it changes in a way that a new pixmap is required.
     *
     * @see isDiscarded
     */
    void markAsDiscarded();
    /**
     * The size of the pixmap.
     */
    const QSize &size() const;
    /**
     * The geometry of the Client's content inside the pixmap. In case of a decorated Client the
     * pixmap also contains the decoration which is not rendered into this pixmap, though. This
     * contentsRect tells where inside the complete pixmap the real content is.
     */
    const QRect &contentsRect() const;
    /**
     * @brief Returns the Toplevel this WindowPixmap belongs to.
     * Note: the Toplevel can change over the lifetime of the WindowPixmap in case the Toplevel is copied to Deleted.
     */
    Toplevel *toplevel() const;

    /**
     * @returns the parent WindowPixmap in the sub-surface tree
     **/
    WindowPixmap *parent() const {
        return m_parent;
    }

    /**
     * @returns the current sub-surface tree
     **/
    QVector<WindowPixmap*> children() const {
        return m_children;
    }

    /**
     * @returns the subsurface this WindowPixmap is for if it is not for a root window
     **/
    QPointer<KWayland::Server::SubSurfaceInterface> subSurface() const {
        return m_subSurface;
    }

    /**
     * @returns the surface this WindowPixmap references, might be @c null.
     **/
    KWayland::Server::SurfaceInterface *surface() const;

protected:
    explicit WindowPixmap(Scene::Window *window);
    explicit WindowPixmap(const QPointer<KWayland::Server::SubSurfaceInterface> &subSurface, WindowPixmap *parent);
    virtual WindowPixmap *createChild(const QPointer<KWayland::Server::SubSurfaceInterface> &subSurface);
    /**
     * @return The Window this WindowPixmap belongs to
     */
    Scene::Window *window();

    /**
     * Should be called by the implementing subclasses when the Wayland Buffer changed and needs
     * updating.
     **/
    virtual void updateBuffer();

    /**
     * Sets the sub-surface tree to @p children.
     **/
    void setChildren(const QVector<WindowPixmap*> &children) {
        m_children = children;
    }

private:
    Scene::Window *m_window;
    xcb_pixmap_t m_pixmap;
    QSize m_pixmapSize;
    bool m_discarded;
    QRect m_contentsRect;
    QPointer<KWayland::Server::BufferInterface> m_buffer;
    QSharedPointer<QOpenGLFramebufferObject> m_fbo;
    WindowPixmap *m_parent = nullptr;
    QVector<WindowPixmap*> m_children;
    QPointer<KWayland::Server::SubSurfaceInterface> m_subSurface;
};

class Scene::EffectFrame
{
public:
    EffectFrame(EffectFrameImpl* frame);
    virtual ~EffectFrame();
    virtual void render(QRegion region, double opacity, double frameOpacity) = 0;
    virtual void free() = 0;
    virtual void freeIconFrame() = 0;
    virtual void freeTextFrame() = 0;
    virtual void freeSelection() = 0;
    virtual void crossFadeIcon() = 0;
    virtual void crossFadeText() = 0;

protected:
    EffectFrameImpl* m_effectFrame;
};

inline
int Scene::Window::x() const
{
    return toplevel->x();
}

inline
int Scene::Window::y() const
{
    return toplevel->y();
}

inline
int Scene::Window::width() const
{
    return toplevel->width();
}

inline
int Scene::Window::height() const
{
    return toplevel->height();
}

inline
QRect Scene::Window::geometry() const
{
    return toplevel->geometry();
}

inline
QSize Scene::Window::size() const
{
    return toplevel->size();
}

inline
QPoint Scene::Window::pos() const
{
    return toplevel->pos();
}

inline
QRect Scene::Window::rect() const
{
    return toplevel->rect();
}

inline
Toplevel* Scene::Window::window() const
{
    return toplevel;
}

inline
void Scene::Window::updateToplevel(Toplevel* c)
{
    toplevel = c;
}

inline
const Shadow* Scene::Window::shadow() const
{
    return m_shadow;
}

inline
Shadow* Scene::Window::shadow()
{
    return m_shadow;
}

inline
QPointer<KWayland::Server::BufferInterface> WindowPixmap::buffer() const
{
    return m_buffer;
}

inline
const QSharedPointer<QOpenGLFramebufferObject> &WindowPixmap::fbo() const
{
    return m_fbo;
}

template <typename T>
inline
T* Scene::Window::windowPixmap()
{
    if (m_currentPixmap.isNull()) {
        m_currentPixmap.reset(createWindowPixmap());
    }
    if (m_currentPixmap->isValid()) {
        return static_cast<T*>(m_currentPixmap.data());
    }
    m_currentPixmap->create();
    if (m_currentPixmap->isValid()) {
        return static_cast<T*>(m_currentPixmap.data());
    } else {
        return static_cast<T*>(m_previousPixmap.data());
    }
}

template <typename T>
inline
T* Scene::Window::previousWindowPixmap()
{
    return static_cast<T*>(m_previousPixmap.data());
}

inline
Toplevel* WindowPixmap::toplevel() const
{
    return m_window->window();
}

inline
xcb_pixmap_t WindowPixmap::pixmap() const
{
    return m_pixmap;
}

inline
bool WindowPixmap::isDiscarded() const
{
    return m_discarded;
}

inline
void WindowPixmap::markAsDiscarded()
{
    m_discarded = true;
    m_window->referencePreviousPixmap();
}

inline
const QRect &WindowPixmap::contentsRect() const
{
    return m_contentsRect;
}

inline
const QSize &WindowPixmap::size() const
{
    return m_pixmapSize;
}

} // namespace

Q_DECLARE_INTERFACE(KWin::SceneFactory, "org.kde.kwin.Scene")

#endif

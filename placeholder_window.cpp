#include "placeholder_window.h"
#include <KKeyServer>
#include "wayland-server.h"
#include "utils.h"
#include <QVector>
#include <xcb/shape.h>
#include "screens.h"

namespace KWin
{
    PlaceholderWindow::PlaceholderWindow()
        : X11EventFilter(QVector<int>{XCB_EXPOSE, XCB_EVENT_MASK_KEY_PRESS, XCB_VISIBILITY_NOTIFY}), m_window(XCB_WINDOW_NONE), m_shapeXRects(nullptr), m_shapeXRectsCount(0), m_waylandServer(nullptr)
    {
    }

    PlaceholderWindow::~PlaceholderWindow()
    {
    }

    bool PlaceholderWindow::create(const QRect &rc, WaylandServer *ws)
    {
        m_waylandServer = ws;
        if (m_waylandServer != nullptr)
        { //暂时不支持
            return false;
        }

        if (!Xcb::Extensions::self()->isShapeInputAvailable()) // needed in setupOverlay()
            return false;

        destroy();

        uint32_t mask = XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
        uint32_t values[] = {
            true,
            XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS};
        m_window.create(rc, XCB_WINDOW_CLASS_INPUT_OUTPUT, mask, values);
        m_window.map();

        foreground = xcb_generate_id(connection());
        mask = XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;
        values[0] = 0xff7f7f7f;
        values[1] = 0;
        xcb_create_gc(connection(), foreground, rootWindow(), mask, values);

        setShape();

        return true;
    }

    // bool PlaceholderWindow::DragingWithoutContent() const
    // {
    //     KConfigGroup kwinConfig(KSharedConfig::openConfig("kwinrc"), "Worksapce");
    //     return kwinConfig.readEntry("ShowWindowCotentDragging", true);
    // }
    void PlaceholderWindow::setShape()
    {
        QRect rc = m_window.geometry();
        int h = rc.height();
        int w = rc.width();

        rc.setLeft(0);
        rc.setTop(0);

        rc.setHeight(h);
        rc.setWidth(w);

        QRect innerRect = rc;
        innerRect.adjust(5, 5, -5, -5);

        QRegion region;
        QRegion innerRegion(innerRect);
        QRegion outerRegion(rc);

        QRegion borderRect = outerRegion.xored(innerRegion); //取两块区域都不相交的区域，既仅获得方块的边框区域

        if (m_shapeXRects)
            delete[] m_shapeXRects;

        QVector<QRect> rects = borderRect.rects();
        m_shapeXRectsCount = rects.count();
        m_shapeXRects = new xcb_rectangle_t[m_shapeXRectsCount];
        for (int i = 0; i < rects.count(); ++i)
        {
            m_shapeXRects[i].x = rects[i].x();
            m_shapeXRects[i].y = rects[i].y();
            m_shapeXRects[i].width = rects[i].width();
            m_shapeXRects[i].height = rects[i].height();
        }
        xcb_shape_rectangles(connection(), XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING, XCB_CLIP_ORDERING_UNSORTED,
                             m_window, 0, 0, rects.count(), m_shapeXRects);
        //delete[] m_shapeXRects;
        xcb_shape_rectangles(connection(), XCB_SHAPE_SO_SET, XCB_SHAPE_SK_INPUT, XCB_CLIP_ORDERING_UNSORTED, m_window, 0, 0, 0, NULL);
    }

    quint32 PlaceholderWindow::window() const
    {
        return m_window;
    }
    void PlaceholderWindow::move(uint32_t x, uint32_t y)
    {

        m_window.move(x, y);
    }
    //要设计形状 和位置
    void PlaceholderWindow::setGeometry(const QRect &rc)
    {
        QRect currc = m_window.geometry();
        if (currc.size() == rc.size())
        {
            m_window.move(rc.x(), rc.y());
        }
        else
        {
            m_window.setGeometry(rc);
            setShape();
        }
    }
    void PlaceholderWindow::destroy()
    {

        if (m_window == XCB_WINDOW_NONE)
        {
            return;
        }

        //  m_window.unmap();
        m_window.reset();

        if (m_shapeXRects != nullptr)
        {
            delete[] m_shapeXRects;
            m_shapeXRects = nullptr;
        }
        m_window = XCB_WINDOW_NONE;
    }
    bool PlaceholderWindow::event(xcb_generic_event_t *event)
    {

        const uint8_t eventType = event->response_type & ~0x80;
        if (eventType == XCB_EXPOSE)
        {
            const auto *expose = reinterpret_cast<xcb_expose_event_t *>(event);
            if (m_window != XCB_WINDOW_NONE && expose->window == m_window)
            {

                // QVector< QRect > rects = m_shape.rects();

                //xcb_rectangle_t *xrects = new xcb_rectangle_t[rects.count()];

                if (m_shapeXRects)
                {
                    xcb_poly_fill_rectangle(connection(), m_window, foreground, m_shapeXRectsCount, m_shapeXRects);
                }
                return true;
            }
        }
        else if (eventType == XCB_VISIBILITY_NOTIFY)
        {
            //const auto *visibility = reinterpret_cast<xcb_visibility_notify_event_t*>(event);
            //if (m_window != XCB_WINDOW_NONE && visibility->window == m_window) {
            // bool was_visible = isVisible();
            // setVisibility((visibility->state != XCB_VISIBILITY_FULLY_OBSCURED));
            // auto compositor = Compositor::self();
            // if (!was_visible && m_visible) {
            //     // hack for #154825
            //     compositor->addRepaintFull();
            //     QTimer::singleShot(2000, compositor, &Compositor::addRepaintFull);
            // }
            // compositor->scheduleRepaint();
            //}
        }
        else if (eventType == XCB_KEY_PRESS)
        {
            // int keyQt;

            // xcb_key_press_event_t *kp = (xcb_key_press_event_t *)event;
            // KKeyServer::xcbKeyPressEventToQt(event, &keyQt);

            // if(keyQt == Qt::Key_Escape){
            //     //取消当前的拖动
            //     int  ffffff=0;
            // }

            //return true;
        }
        return false;
    }

} // namespace KWin

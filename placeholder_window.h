/********************************************************************
 * PlaceholderWindow 类 
 * 窗口移动时候，如果不显示移动窗口的内容 ，需要实例化这个窗口，
 * 这个窗口会随着鼠标移动。
 * 当取消移动（ESC ），这个窗口释放
 * 当移动结束（ 鼠标button release） ， 源窗口的要移动到当前窗口的位置
 
*********************************************************************/

#ifndef KWIN_PLACEHOLDER_WINDDOW_H
#define KWIN_PLACEHOLDER_WINDDOW_H


#include "x11eventfilter.h"
#include "xcbutils.h"
#include <xcb/xcb.h>



namespace KWin {
class WaylandServer;

class KWIN_EXPORT PlaceholderWindow : public X11EventFilter {
public:
    PlaceholderWindow();
    ~PlaceholderWindow();

public:

    quint32 window() const;

    bool create(const  QRect& rc,WaylandServer *ws=nullptr);
    void destroy();

    bool event(xcb_generic_event_t *event) ;
    void move (uint32_t x,uint32_t y);

    void setGeometry(const QRect &rc);

    
private:

    void setShape();

 
    
private:
    Xcb::Window m_window;
    xcb_rectangle_t *m_shapeXRects;
    int m_shapeXRectsCount;
    xcb_gcontext_t       foreground;

    WaylandServer *m_waylandServer;


};
} // namespace

#endif //KWIN_PLACEHOLDER_WINDDOW_H

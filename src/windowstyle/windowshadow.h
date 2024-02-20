#ifndef WINDOWSHADOW_H
#define WINDOWSHADOW_H 

#include <QObject>
#include <QPointF>
#include <QColor>
#include <QImage>
#include <QMargins>
#include <QMap>
#include <QVector>

namespace KWin
{
class Window;

struct shadowConfig
{
    QPointF windowRadius;
    QPointF shadowOffset;
    QColor  shadowColor;
    qreal   shadowRadius;
    qreal   borderWidth;
    QColor  borderColor;
};

class WindowShadow : public QObject
{
    Q_OBJECT
public:
    explicit WindowShadow(Window *window);
    ~WindowShadow();

    void updateWindowShadow();
    QPointF getWindowRadius();

    static QString buildShadowCacheKey(shadowConfig &config);
    /*static*/ void getShadow();

public:
    QImage  kwin_popup_shadow_top,
            kwin_popup_shadow_top_right,
            kwin_popup_shadow_right,
            kwin_popup_shadow_bottom_right,
            kwin_popup_shadow_bottom,
            kwin_popup_shadow_bottom_left,
            kwin_popup_shadow_left,
            kwin_popup_shadow_top_left;

    QMargins m_padding;
    QString  m_key;
    static QMap<QString, QVector<QImage>> m_cacheShadow;
private:
    Window *m_window;
    
};
}
#endif
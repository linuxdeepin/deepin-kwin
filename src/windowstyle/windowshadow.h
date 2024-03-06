/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 zhang yu <zhangyud@uniontech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
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

    void            updateWindowShadow();
    QPointF         getWindowRadius();
    QString         getDefaultShadowColor();
    qreal           getDefaultShadowRadius();
    QString         getDefaultBorderColor();
    float           getDefaultShadowOffset();

    bool            isSpecialWindow();

    static QString  buildShadowCacheKey(shadowConfig &config);
    void            getShadow();
    void            resetShadowKey();
    QString         getShadowKey();
    QMargins        getPadding();

public:
    static QMap<QString, QVector<QImage>> m_cacheShadow;
private:
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
    Window  *m_window;
};
}
#endif
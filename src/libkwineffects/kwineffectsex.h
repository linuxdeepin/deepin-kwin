/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 zhang yu <zhangyud@uniontech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWINEFFECTSEX_H
#define KWINEFFECTSEX_H

#include <kwineffects.h>
#include <kwinconfig.h>
#include <kwineffects_export.h>
#include <kwinglobals.h>

/**
 * Logging category to be used inside the KWin effects.
 * Do not use in this library.
 **/
//Q_DECLARE_LOGGING_CATEGORY(KWINEFFECTS)

namespace KWin
{

class EffectWindow;
class Output;

#define KWIN_EFFECT_API_MAKE_VERSION( major, minor ) (( major ) << 8 | ( minor ))
#define KWIN_EFFECT_API_VERSION_MAJOR 0
#define KWIN_EFFECT_API_VERSION_MINOR 227
#define KWIN_EFFECT_API_VERSION KWIN_EFFECT_API_MAKE_VERSION( \
        KWIN_EFFECT_API_VERSION_MAJOR, KWIN_EFFECT_API_VERSION_MINOR )

class KWINEFFECTS_EXPORT EffectFrameEx : public EffectFrame
{
    // Q_OBJECT
public:
    explicit EffectFrameEx();
    virtual ~EffectFrameEx();

    virtual void  setColor(QColor &color) = 0;
    virtual QColor &color() const = 0;
    virtual void setRadius(int radius) = 0;
    virtual int &radius() = 0;
    virtual void setImage(const QUrl &image) = 0;
    virtual void setImage(const QPixmap &image) = 0;
    virtual const QUrl &image() const = 0;

// Q_SIGNALS:

};

class KWINEFFECTS_EXPORT EffectsHandlerEx : public EffectsHandler
{
    Q_OBJECT
public:
    explicit EffectsHandlerEx(CompositingType type);
    virtual ~EffectsHandlerEx();

    virtual void setQuickTileWindow(KWin::EffectWindow *w, int mode) = 0;
    virtual int getQuickTileMode(KWin::EffectWindow *w) = 0;
    virtual QRectF getQuickTileGeometry(KWin::EffectWindow *w, int mode, QPointF pos) = 0;
    virtual QString getActiveColor() = 0;
    virtual void setKeepAbove(KWin::EffectWindow *c, bool) = 0;
    virtual void requestLock() = 0;
    virtual void sendPointer(Qt::MouseButton) = 0;
    virtual void changeBlurState(bool) = 0;
    virtual EffectWindowList getChildWinList(KWin::EffectWindow *w) = 0;
    virtual bool isTransientWin(KWin::EffectWindow *w) = 0;
    virtual EffectScreen *getCurrentPaintingScreen() = 0;
    virtual bool isShortcuts(QKeyEvent *event) = 0;

    virtual std::unique_ptr<EffectFrameEx> effectFrameEx(QString url, bool staticSize = true, 
                                                     const QPoint &position = QPoint(-1, -1),
                                                     Qt::Alignment alignment = Qt::AlignCenter) const = 0;
    virtual EffectScreen *findScreen(Output *output) const = 0;

Q_SIGNALS:
    void triggerSplitPreview(KWin::EffectWindow *w);
    void swapSplitWin(KWin::EffectWindow *w, int index);

    void showMultitasking();

};

/**
 * Pointer to the global EffectsHandlerEx object.
 **/
extern KWINEFFECTS_EXPORT EffectsHandlerEx* effectsEx;

} // namespace

#endif // KWINEFFECTSEX_H

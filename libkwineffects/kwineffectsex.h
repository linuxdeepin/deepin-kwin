/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>
Copyright (C) 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
Copyright (C) 2018 Vlad Zagorodniy <vladzzag@gmail.com>

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

#define KWIN_EFFECT_API_MAKE_VERSION( major, minor ) (( major ) << 8 | ( minor ))
#define KWIN_EFFECT_API_VERSION_MAJOR 0
#define KWIN_EFFECT_API_VERSION_MINOR 227
#define KWIN_EFFECT_API_VERSION KWIN_EFFECT_API_MAKE_VERSION( \
        KWIN_EFFECT_API_VERSION_MAJOR, KWIN_EFFECT_API_VERSION_MINOR )


class KWINEFFECTS_EXPORT EffectsHandlerEx : public EffectsHandler
{
    Q_OBJECT
public:
    explicit EffectsHandlerEx(CompositingType type);
    virtual ~EffectsHandlerEx();

    virtual bool checkWindowAllowToSplit(KWin::EffectWindow *c) = 0;
    virtual QString getActiveColor() = 0;
    virtual void setSplitWindow(KWin::EffectWindow* c, int mode, bool isShowPreview = false) = 0;
    virtual SwipeDirection desktopChangedDirection() const = 0;
    virtual void resetSplitOutlinePos(int screen, int desktop) = 0;
    /**
     * Finds the EffectWindow for the internal window @p w.
     * If there is no such window @c null is returned.
     *
     * On Wayland this returns the internal window. On X11 it returns an Unamanged with the
     * window id matching that of the provided window @p w.
     *
     */
    virtual EffectWindow *findWindow(const QUuid &w) const = 0;
    virtual void setKeepAbove(KWin::EffectWindow *c, bool) = 0;
    virtual QString getScreenNameForWayland(int screen) = 0;
    virtual void requestLock() = 0;
    virtual void sendPointer(Qt::MouseButton) = 0;
    virtual void changeBlurState(bool) = 0;
    virtual EffectWindowList getChildWinList(KWin::EffectWindow *w) = 0;
    virtual bool isTransientWin(KWin::EffectWindow *w) = 0;
    virtual int getCurrentPaintingScreen() = 0;
    virtual bool isShortcuts(QKeyEvent *event) = 0;
    virtual Effect *findEffect(const QString &name) const = 0;

Q_SIGNALS:
    void windowQuickTileModeChanged(KWin::EffectWindow *w);
    void showSplitScreenPreview(KWin::EffectWindow *w);
    void swapSplitWin(KWin::EffectWindow *w, int index);

    void showMultitasking();

};

/**
 * Pointer to the global EffectsHandlerEx object.
 **/
extern KWINEFFECTS_EXPORT EffectsHandlerEx* effectsEx;


} // namespace

#endif // KWINEFFECTSEX_H

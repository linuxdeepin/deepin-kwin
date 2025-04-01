/*
    SPDX-FileCopyrightText: 2019 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "windoweffects.h"
#include "effects.h"

#include <QGuiApplication>
#include <QWidget>
#include <QWindow>

Q_DECLARE_METATYPE(KWindowEffects::SlideFromLocation)

namespace KWin
{

WindowEffects::WindowEffects()
    : QObject()
    , KWindowEffectsPrivate()
{
}

WindowEffects::~WindowEffects()
{
}

namespace
{
QWindow *findWindow(WId win)
{
    const auto windows = qApp->allWindows();
    auto it = std::find_if(windows.begin(), windows.end(), [win](QWindow *w) {
        return w->winId() == win;
    });
    if (it == windows.end()) {
        return nullptr;
    }
    return *it;
}
}

bool WindowEffects::isEffectAvailable(KWindowEffects::Effect effect)
{
    if (!effects) {
        return false;
    }
    auto e = static_cast<EffectsHandlerImpl *>(effects);
    switch (effect) {
    case KWindowEffects::BackgroundContrast:
        return e->isEffectLoaded(QStringLiteral("contrast"));
    case KWindowEffects::BlurBehind:
        return e->isEffectLoaded(QStringLiteral("blur"));
    case KWindowEffects::Slide:
        return e->isEffectLoaded(QStringLiteral("slidingpopups"));
    default:
        // plugin does not provide integration for other effects
        return false;
    }
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void WindowEffects::slideWindow(WId id, KWindowEffects::SlideFromLocation location, int offset)
{
    auto w = findWindow(id);
    if (!w) {
        return;
    }
    w->setProperty("kwin_slide", QVariant::fromValue(location));
    w->setProperty("kwin_slide_offset", offset);
}
#else
void WindowEffects::slideWindow(QWindow *window, KWindowEffects::SlideFromLocation location, int offset)
{
    window->setProperty("kwin_slide", QVariant::fromValue(location));
    window->setProperty("kwin_slide_offset", offset);
}
#endif

#if KWINDOWSYSTEM_BUILD_DEPRECATED_SINCE(5, 81) && QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
QList<QSize> WindowEffects::windowSizes(const QList<WId> &ids)
{
    return {};
}
#endif

#if KWINDOWSYSTEM_BUILD_DEPRECATED_SINCE(5, 82) && QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void WindowEffects::presentWindows(WId controller, const QList<WId> &ids)
{
}
#endif

#if KWINDOWSYSTEM_BUILD_DEPRECATED_SINCE(5, 82) && QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void WindowEffects::presentWindows(WId controller, int desktop)
{
}
#endif

#if KWINDOWSYSTEM_BUILD_DEPRECATED_SINCE(5, 82) && QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void WindowEffects::highlightWindows(WId controller, const QList<WId> &ids)
{
}
#endif


#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void WindowEffects::enableBlurBehind(WId window, bool enable, const QRegion &region)
{
    auto w = findWindow(window);
    if (!w) {
        return;
    }
    if (enable) {
        w->setProperty("kwin_blur", region);
    } else {
        w->setProperty("kwin_blur", {});
    }
}

void WindowEffects::enableBackgroundContrast(WId window, bool enable, qreal contrast, qreal intensity, qreal saturation, const QRegion &region)
{
    auto w = findWindow(window);
    if (!w) {
        return;
    }
    if (enable) {
        w->setProperty("kwin_background_region", region);
        w->setProperty("kwin_background_contrast", contrast);
        w->setProperty("kwin_background_intensity", intensity);
        w->setProperty("kwin_background_saturation", saturation);
    } else {
        w->setProperty("kwin_background_region", {});
        w->setProperty("kwin_background_contrast", {});
        w->setProperty("kwin_background_intensity", {});
        w->setProperty("kwin_background_saturation", {});
    }
}
#else
void WindowEffects::enableBlurBehind(QWindow *window, bool enable, const QRegion &region)
{
    if (enable) {
        window->setProperty("kwin_blur", region);
    } else {
        window->setProperty("kwin_blur", {});
    }
}

void WindowEffects::enableBackgroundContrast(QWindow *window, bool enable, qreal contrast, qreal intensity, qreal saturation, const QRegion &region)
{
    if (enable) {
        window->setProperty("kwin_background_region", region);
        window->setProperty("kwin_background_contrast", contrast);
        window->setProperty("kwin_background_intensity", intensity);
        window->setProperty("kwin_background_saturation", saturation);
    } else {
        window->setProperty("kwin_background_region", {});
        window->setProperty("kwin_background_contrast", {});
        window->setProperty("kwin_background_intensity", {});
        window->setProperty("kwin_background_saturation", {});
    }
}
#endif

#if KWINDOWSYSTEM_BUILD_DEPRECATED_SINCE(5, 67) && QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void WindowEffects::markAsDashboard(WId window)
{
}
#endif

}

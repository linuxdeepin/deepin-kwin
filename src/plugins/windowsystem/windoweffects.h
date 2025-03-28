/*
    SPDX-FileCopyrightText: 2019 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#pragma once
#include <QObject>
#include <kwindowsystem_version.h>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <KWindowSystem/private/kwindoweffects_p.h>
#else
#include <private/kwindoweffects_p.h>
#endif

namespace KWin
{

class WindowEffects : public QObject, public KWindowEffectsPrivate
{
public:
    WindowEffects();
    ~WindowEffects() override;

    bool isEffectAvailable(KWindowEffects::Effect effect) override;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    void slideWindow(WId id, KWindowEffects::SlideFromLocation location, int offset) override;
#else
    void slideWindow(QWindow *window, KWindowEffects::SlideFromLocation location, int offset) override;
#endif


#if KWINDOWSYSTEM_BUILD_DEPRECATED_SINCE(5, 81) && QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QList<QSize> windowSizes(const QList<WId> &ids) override;
#endif

#if KWINDOWSYSTEM_BUILD_DEPRECATED_SINCE(5, 82) && QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    void presentWindows(WId controller, const QList<WId> &ids) override;
    void presentWindows(WId controller, int desktop = NET::OnAllDesktops) override;
    void highlightWindows(WId controller, const QList<WId> &ids) override;
#endif

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    void enableBlurBehind(WId window, bool enable = true, const QRegion &region = QRegion()) override;
    void enableBackgroundContrast(WId window, bool enable = true, qreal contrast = 1, qreal intensity = 1, qreal saturation = 1, const QRegion &region = QRegion()) override;
#else
    void enableBlurBehind(QWindow *window, bool enable = true, const QRegion &region = QRegion()) override;
    void enableBackgroundContrast(QWindow *window, bool enable = true, qreal contrast = 1, qreal intensity = 1, qreal saturation = 1, const QRegion &region = QRegion()) override;
#endif

#if KWINDOWSYSTEM_BUILD_DEPRECATED_SINCE(5, 67) && QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    void markAsDashboard(WId window) override;
#endif
};

}

/*
    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "showfpseffect.h"

#include <QQuickItem>
#include <QWindow>

namespace KWin
{

ScreenFps::ScreenFps(QObject *parent)
    : QObject(parent)
{
}

int ScreenFps::fps() const
{
    return m_fps;
}

int ScreenFps::maximumFps() const
{
    return m_maximumFps;
}

int ScreenFps::paintDuration() const
{
    return m_paintDuration;
}

int ScreenFps::paintAmount() const
{
    return m_paintAmount;
}

QColor ScreenFps::paintColor() const
{
    auto normalizedDuration = std::min(1.0, m_paintDuration / 100.0);
    return QColor::fromHsvF(0.3 - (0.3 * normalizedDuration), 1.0, 1.0);
}

void ScreenFps::countFrame()
{
    m_frames += 1;
}

void ScreenFps::updateFps(int maximumFps)
{
    if (maximumFps != m_maximumFps) {
        m_maximumFps = maximumFps;
        Q_EMIT maximumFpsChanged();
    }

    const auto now = std::chrono::steady_clock::now();
    if (m_lastUpdate == std::chrono::steady_clock::time_point()) {
        m_lastUpdate = now;
        return;
    }
    const auto elapsed = now - m_lastUpdate;
    if (elapsed < std::chrono::seconds(1)) {
        return;
    }
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    const int fps = qRound(m_frames * 1000.0 / elapsedMs);
    m_frames = 0;
    m_lastUpdate = now;
    if (fps != m_fps) {
        m_fps = fps;
        Q_EMIT fpsChanged();
    }
}

void ScreenFps::beginPaint()
{
    m_paintDurationTimer.restart();
    m_paintAmount = 0;
}

void ScreenFps::endPaint()
{
    m_paintDuration = m_paintDurationTimer.elapsed();
    Q_EMIT paintChanged();
}

void ScreenFps::addPaintAmount(int amount)
{
    m_paintAmount += amount;
}

ShowFpsEffect::ShowFpsEffect()
{
    connect(effects, &EffectsHandler::screenRemoved, this, [this](EffectScreen *screen) {
        if (m_paintedScreen == screen) {
            m_paintedScreen = nullptr;
        }
        delete m_screenEntries.take(screen);
    });
}

ShowFpsEffect::~ShowFpsEffect()
{
    qDeleteAll(m_screenEntries);
}

ShowFpsEffect::ScreenEntry *ShowFpsEffect::ensureScreenEntry(EffectScreen *screen)
{
    if (!screen) {
        return nullptr;
    }
    if (ScreenEntry *entry = m_screenEntries.value(screen)) {
        return entry;
    }

    if (!m_window) {
        m_window = std::make_unique<QWindow>();
        m_window->create();
    }
    auto entry = std::make_unique<ScreenEntry>();
    entry->data = std::make_unique<ScreenFps>();
    entry->scene = std::make_unique<OffscreenQuickScene>(nullptr, m_window.get());
    const auto url = QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/effects/showfps/qml/main.qml")));
    entry->scene->setSource(url, {{QStringLiteral("effect"), QVariant::fromValue(entry->data.get())}});
    m_screenEntries.insert(screen, entry.get());
    return entry.release();
}

void ShowFpsEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    effects->prePaintScreen(data, presentTime);

    m_paintedScreen = data.screen;
    if (ScreenEntry *entry = ensureScreenEntry(data.screen)) {
        entry->data->beginPaint();
        entry->data->countFrame();

        const auto rect = effects->renderTargetRect();
        const int padding = 10;
        // Size the overlay to the QML implicit size. The QML reserves a
        // fixed-width slot for the FPS value, so the size (and every text
        // position) only changes if the font environment changes, not when
        // the reported values change. A fixed fallback keeps a usable box
        // if the QML failed to load.
        QSize contentSize(entry->scene->rootItem()->implicitWidth(), entry->scene->rootItem()->implicitHeight());
        if (contentSize.isEmpty()) {
            contentSize = QSize(320, 140);
        }
        entry->scene->setGeometry(QRect(rect.x() + rect.width() - contentSize.width() - padding, rect.y() + padding, contentSize.width(), contentSize.height()));
    }
}

void ShowFpsEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    effects->paintScreen(mask, region, data);

    if (EffectScreen *screen = data.screen()) {
        if (ScreenEntry *entry = m_screenEntries.value(screen)) {
            entry->data->updateFps(screen->refreshRate() / 1000); // Convert from mHz to Hz.
            effects->renderOffscreenQuickView(entry->scene.get());
        }
    }
}

void ShowFpsEffect::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    effects->paintWindow(w, mask, region, data);

    if (ScreenEntry *entry = m_screenEntries.value(m_paintedScreen)) {
        // Take intersection of region and actual window's rect, minus the fps area
        //  (since we keep repainting it) and count the pixels.
        QRegion repaintRegion = region & w->frameGeometry().toRect();
        repaintRegion -= entry->scene->geometry();
        for (const QRect &rect : repaintRegion) {
            entry->data->addPaintAmount(rect.width() * rect.height());
        }
    }
}

void ShowFpsEffect::postPaintScreen()
{
    effects->postPaintScreen();

    if (ScreenEntry *entry = m_screenEntries.value(m_paintedScreen)) {
        entry->data->endPaint();
        effects->addRepaint(entry->scene->geometry());
    }
}

bool ShowFpsEffect::supported()
{
    return effects->isOpenGLCompositing();
}

} // namespace KWin

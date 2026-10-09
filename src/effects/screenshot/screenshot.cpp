/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2010 Nokia Corporation and /or its subsidiary(-ies)
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screenshot.h"
#include "screenshotdbusinterface1.h"
#include "screenshotdbusinterface2.h"

#include "composite.h"
#include "core/output.h"
#include "wayland/dderestrict_interface.h"
#include "wayland_server.h"

#include <kwinglplatform.h>
#include <kwinglutils.h>

#include <QPainter>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#else
#include <private/qtx11extras_p.h>
#endif

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>

#define ProhibitScreenshotDBusService "com.deepin.prohibitscreenshot"
#define ProhibitScreenshotDBusPath "/com/deepin/prohibitscreenshot"
#define ProhibitScreenshotDBusInterface "deepin.prohibitscreenshot.interface"

namespace KWin
{

struct ScreenShotWindowData
{
    QFutureInterface<QImage> promise;
    ScreenShotFlags flags;
    EffectWindow *window = nullptr;
};

struct ScreenShotAreaData
{
    QFutureInterface<QImage> promise;
    ScreenShotFlags flags;
    QRect area;
    QImage result;
    QList<EffectScreen *> screens;
};

struct ScreenShotScreenData
{
    QFutureInterface<QImage> promise;
    ScreenShotFlags flags;
    EffectScreen *screen = nullptr;
};

struct ScreenShotWindowSizedData
{
    QFutureInterface<QImage> promise;
    ScreenShotFlags flags;
    QSize size;
    EffectWindow *window = nullptr;
};

static void convertFromGLImage(QImage &img, int w, int h)
{
    // from QtOpenGL/qgl.cpp
    // SPDX-FileCopyrightText: 2010 Nokia Corporation and /or its subsidiary(-ies)
    // see https://github.com/qt/qtbase/blob/dev/src/opengl/qgl.cpp
    if (QSysInfo::ByteOrder == QSysInfo::BigEndian) {
        // OpenGL gives RGBA; Qt wants ARGB
        uint *p = reinterpret_cast<uint *>(static_cast<void *>(img.bits()));
        uint *end = p + w * h;
        while (p < end) {
            uint a = *p << 24;
            *p = (*p >> 8) | a;
            p++;
        }
    } else {
        // OpenGL gives ABGR (i.e. RGBA backwards); Qt wants ARGB
        for (int y = 0; y < h; y++) {
            uint *q = reinterpret_cast<uint *>(static_cast<void *>(img.scanLine(y)));
            for (int x = 0; x < w; ++x) {
                const uint pixel = *q;
                *q = ((pixel << 16) & 0xff0000) | ((pixel >> 16) & 0xff)
                    | (pixel & 0xff00ff00);

                q++;
            }
        }
    }
    img = img.mirrored();
}

bool ScreenShotEffect::supported()
{
    return effects->isOpenGLCompositing() && GLFramebuffer::supported();
}

ScreenShotEffect::ScreenShotEffect()
    : m_dbusInterface1(new ScreenShotDBusInterface1(this))
    , m_dbusInterface2(new ScreenShotDBusInterface2(this))
{
    QString checkDbusInterface = QString::fromUtf8(qgetenv("KWIN_CHECK_DBUS_INTERFACES"));
    if (checkDbusInterface.isEmpty() || checkDbusInterface.toLower() == "false") {
        checkInteface = false;
    }
    connect(effects, &EffectsHandler::screenAdded, this, &ScreenShotEffect::handleScreenAdded);
    connect(effects, &EffectsHandler::screenRemoved, this, &ScreenShotEffect::handleScreenRemoved);
    connect(effects, &EffectsHandler::windowClosed, this, &ScreenShotEffect::handleWindowClosed);
    if (waylandServer()) {
        connect(Compositor::self(), &Compositor::captureFrameReady, this, [this](Output *output) {
            takeScreenShots(effectsEx->findScreen(output));
        });
    }
}

ScreenShotEffect::~ScreenShotEffect()
{
    cancelWindowScreenShots();
    cancelAreaScreenShots();
    cancelScreenScreenShots();
}

QFuture<QImage> ScreenShotEffect::scheduleScreenShot(EffectScreen *screen, ScreenShotFlags flags)
{
    for (ScreenShotScreenData &data : m_screenScreenShots) {
        if (data.screen == screen && data.flags == flags) {
            return data.promise.future();
        }
    }

    ScreenShotScreenData data;
    data.screen = screen;
    data.flags = flags;

    m_screenScreenShots.append(data);
    effects->addRepaint(screen->geometry());

    data.promise.reportStarted();
    return data.promise.future();
}

QFuture<QImage> ScreenShotEffect::scheduleScreenShot(const QRect &area, ScreenShotFlags flags)
{
    for (ScreenShotAreaData &data : m_areaScreenShots) {
        if (data.area == area && data.flags == flags) {
            return data.promise.future();
        }
    }

    ScreenShotAreaData data;
    data.area = area;
    data.flags = flags;

    const QList<EffectScreen *> screens = effects->screens();
    for (EffectScreen *screen : screens) {
        if (screen->geometry().intersects(area)) {
            data.screens.append(screen);
        }
    }

    qreal devicePixelRatio = 1.0;
    if (flags & ScreenShotNativeResolution) {
        for (const EffectScreen *screen : std::as_const(data.screens)) {
            if (screen->devicePixelRatio() > devicePixelRatio) {
                devicePixelRatio = screen->devicePixelRatio();
            }
        }
    }

    data.result = QImage(area.size() * devicePixelRatio, QImage::Format_ARGB32_Premultiplied);
    data.result.fill(Qt::transparent);
    data.result.setDevicePixelRatio(devicePixelRatio);

    m_areaScreenShots.append(data);
    effects->addRepaint(area);

    data.promise.reportStarted();
    return data.promise.future();
}

QFuture<QImage> ScreenShotEffect::scheduleScreenShot(EffectWindow *window, ScreenShotFlags flags)
{
    for (ScreenShotWindowData &data : m_windowScreenShots) {
        if (data.window == window && data.flags == flags) {
            return data.promise.future();
        }
    }

    ScreenShotWindowData data;
    data.window = window;
    data.flags = flags;

    m_windowScreenShots.append(data);
    window->addRepaintFull();

    data.promise.reportStarted();
    return data.promise.future();
}

QFuture<QImage> ScreenShotEffect::scheduleScreenShot(EffectWindow *window, const QSize &size, ScreenShotFlags flags)
{
    for (ScreenShotWindowSizedData &data : m_windowScreenSizedShots) {
        if (data.window == window && data.flags == flags && data.size == size) {
            return data.promise.future();
        }
    }

    ScreenShotWindowSizedData data;
    data.window = window;
    data.flags = flags;
    data.size = size;

    m_windowScreenSizedShots.append(data);
    window->addRepaintFull();

    data.promise.reportStarted();
    return data.promise.future();
}

void ScreenShotEffect::cancelWindowScreenShots()
{
    while (!m_windowScreenShots.isEmpty()) {
        ScreenShotWindowData screenshot = m_windowScreenShots.takeLast();
        screenshot.promise.reportCanceled();
    }

    while (!m_windowScreenSizedShots.isEmpty()) {
        ScreenShotWindowSizedData screenshot = m_windowScreenSizedShots.takeLast();
        screenshot.promise.reportCanceled();
    }
}

void ScreenShotEffect::cancelAreaScreenShots()
{
    while (!m_areaScreenShots.isEmpty()) {
        ScreenShotAreaData screenshot = m_areaScreenShots.takeLast();
        screenshot.promise.reportCanceled();
    }
}

void ScreenShotEffect::cancelScreenScreenShots()
{
    while (!m_screenScreenShots.isEmpty()) {
        ScreenShotScreenData screenshot = m_screenScreenShots.takeLast();
        screenshot.promise.reportCanceled();
    }
}

void ScreenShotEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    const bool captureFrame = effects->captureFrameGeneration();
    effects->paintScreen(mask, region, data);

    if (!captureFrame) {
        while (!m_windowScreenShots.isEmpty()) {
            ScreenShotWindowData screenshot = m_windowScreenShots.takeLast();
            takeScreenShot(&screenshot);
        }

        while (!m_windowScreenSizedShots.isEmpty()) {
            ScreenShotWindowSizedData screenshot = m_windowScreenSizedShots.takeLast();
            takeScreenShot(&screenshot);
        }
        if (!waylandServer()) {
            takeScreenShots(data.screen());
        }
    }
}

void ScreenShotEffect::takeScreenShots(EffectScreen *screen)
{
    m_paintedScreen = screen;
    for (int i = m_areaScreenShots.count() - 1; i >= 0; --i) {
        if (takeScreenShot(&m_areaScreenShots[i])) {
            m_areaScreenShots.removeAt(i);
        }
    }
    for (int i = m_screenScreenShots.count() - 1; i >= 0; --i) {
        if (takeScreenShot(&m_screenScreenShots[i])) {
            m_screenScreenShots.removeAt(i);
        }
    }
}

void ScreenShotEffect::takeScreenShot(ScreenShotWindowData *screenshot)
{
    EffectWindow *window = screenshot->window;

    QRect geometry = window->expandedGeometry().toRect();
    if (window->hasDecoration() && !(screenshot->flags & ScreenShotIncludeDecoration)) {
        geometry = window->clientGeometry().toRect();
    }

    ScreenShotWindowSizedData sizedScreenshot;
    sizedScreenshot.promise = screenshot->promise;
    sizedScreenshot.flags = screenshot->flags;
    sizedScreenshot.window = screenshot->window;
    sizedScreenshot.size = geometry.size();

    takeScreenShot(&sizedScreenshot);
}

void ScreenShotEffect::takeScreenShot(ScreenShotWindowSizedData *screenshot)
{
    EffectWindow *window = screenshot->window;

    if (isGlobalProhibitScreenshot() || isProhibitScreenshot(window->windowId())) {
        screenshot->promise.reportResult(getProhibitShotImage());
        screenshot->promise.reportFinished();
        return;
    }

    WindowPaintData d;
    QRectF geometry = window->frameGeometry();
    qreal devicePixelRatio = 1;
    if (window->hasDecoration() && !(screenshot->flags & ScreenShotIncludeDecoration)) {
        geometry = window->frameGeometry();
    }
    if (screenshot->flags & ScreenShotNativeResolution) {
        if (const EffectScreen *screen = window->screen()) {
            devicePixelRatio = screen->devicePixelRatio();
        }
    }
    bool validTarget = true;
    std::unique_ptr<GLTexture> offscreenTexture;
    std::unique_ptr<GLFramebuffer> target;
    if (effects->isOpenGLCompositing()) {
        float fscale = 1.f;
        if (geometry.width() >= geometry.height()) {
            fscale = (float)screenshot->size.width() / (float)geometry.width();
        } else {
            fscale = (float)screenshot->size.height() / (float)geometry.height();
        }

        int w = geometry.width() * fscale;
        int h = geometry.height() * fscale;

        offscreenTexture.reset(new GLTexture(GL_RGBA8, QSize(w, h)));
        offscreenTexture->setFilter(GL_LINEAR);
        offscreenTexture->setWrapMode(GL_CLAMP_TO_EDGE);
        target.reset(new GLFramebuffer(offscreenTexture.get()));
        validTarget = target->valid();
    }
    if (validTarget) {
        d.setXTranslation(-geometry.x());
        d.setYTranslation(-geometry.y());
        d.setRenderTargetScale(devicePixelRatio);

        // render window into offscreen texture
        int mask = PAINT_WINDOW_TRANSFORMED | PAINT_WINDOW_TRANSLUCENT;
        QImage img;
        if (effects->isOpenGLCompositing()) {
            GLFramebuffer::pushFramebuffer(target.get());
            glClearColor(0.0, 0.0, 0.0, 0.0);
            glClear(GL_COLOR_BUFFER_BIT);
            glClearColor(0.0, 0.0, 0.0, 1.0);

            QMatrix4x4 projection;
            projection.ortho(QRect(0, 0, geometry.width() * devicePixelRatio, geometry.height() * devicePixelRatio));
            d.setProjectionMatrix(projection);

            effects->drawWindow(window, mask, infiniteRegion(), d);

            // copy content from framebuffer into image
            img = QImage(offscreenTexture->size(), QImage::Format_ARGB32);
            img.setDevicePixelRatio(devicePixelRatio);
            glReadnPixels(0, 0, img.width(), img.height(), GL_RGBA, GL_UNSIGNED_BYTE, img.sizeInBytes(),
                          static_cast<GLvoid *>(img.bits()));
            GLFramebuffer::popFramebuffer();
            convertFromGLImage(img, img.width(), img.height());
        }

        if (screenshot->flags & ScreenShotIncludeCursor) {
            grabPointerImage(img, geometry.x(), geometry.y());
        }

        screenshot->promise.reportResult(img);
        screenshot->promise.reportFinished();
    } else {
        screenshot->promise.reportCanceled();
    }
}

bool ScreenShotEffect::takeScreenShot(ScreenShotAreaData *screenshot)
{
    if (!effects->waylandDisplay()) {
        // On X11, all screens are painted simultaneously and there is no native HiDPI support.
        QImage snapshot = blitScreenshot(screenshot->area);
        if (screenshot->flags & ScreenShotIncludeCursor) {
            grabPointerImage(snapshot, screenshot->area.x(), screenshot->area.y());
        }
        maskProtectedWindows(snapshot, screenshot->area);
        screenshot->promise.reportResult(snapshot);
        screenshot->promise.reportFinished();
    } else {
        if (!screenshot->screens.contains(m_paintedScreen)) {
            return false;
        }
        screenshot->screens.removeOne(m_paintedScreen);

        const QRect sourceRect = screenshot->area & m_paintedScreen->geometry();
        qreal sourceDevicePixelRatio = 1.0;
        if (screenshot->flags & ScreenShotNativeResolution) {
            sourceDevicePixelRatio = m_paintedScreen->devicePixelRatio();
        }

        const QImage snapshot = blitScreenshot(sourceRect, sourceDevicePixelRatio);
        const QRect nativeArea(screenshot->area.topLeft(),
                               screenshot->area.size() * screenshot->result.devicePixelRatio());

        QPainter painter(&screenshot->result);
        painter.setWindow(nativeArea);
        painter.drawImage(sourceRect, snapshot);
        painter.end();

        if (screenshot->screens.isEmpty()) {
            if (screenshot->flags & ScreenShotIncludeCursor) {
                grabPointerImage(screenshot->result, screenshot->area.x(), screenshot->area.y());
            }
            maskProtectedWindows(screenshot->result, screenshot->area);
            screenshot->promise.reportResult(screenshot->result);
            screenshot->promise.reportFinished();
        }
    }

    return screenshot->promise.isFinished();
}

bool ScreenShotEffect::takeScreenShot(ScreenShotScreenData *screenshot)
{
    if (!m_paintedScreen) {
        return false;
    }

    if (!QX11Info::isPlatformX11() && screenshot->screen != m_paintedScreen) {
        return false;
    }

    qreal devicePixelRatio = 1.0;
    if (screenshot->flags & ScreenShotNativeResolution) {
        devicePixelRatio = screenshot->screen->devicePixelRatio();
    }

    QImage snapshot = blitScreenshot(screenshot->screen->geometry(), devicePixelRatio);
    if (screenshot->flags & ScreenShotIncludeCursor) {
        const int xOffset = screenshot->screen->geometry().x();
        const int yOffset = screenshot->screen->geometry().y();
        grabPointerImage(snapshot, xOffset, yOffset);
    }

    maskProtectedWindows(snapshot, screenshot->screen->geometry());

    screenshot->promise.reportResult(snapshot);
    screenshot->promise.reportFinished();
    return true;
}

QImage ScreenShotEffect::blitScreenshot(const QRect &geometry, qreal devicePixelRatio) const
{
    QImage image;

    if (effects->isOpenGLCompositing()) {
        const QSize nativeSize = geometry.size() * devicePixelRatio;

        std::shared_ptr<GLTexture> captureTexture;
        std::unique_ptr<GLFramebuffer> captureFramebuffer;
        if (waylandServer()) {
            captureTexture = Compositor::self()->captureFrameForOutput(effectsEx->getCurrentPaintingScreen());
            if (!captureTexture) {
                return image;
            }
            captureFramebuffer = std::make_unique<GLFramebuffer>(captureTexture.get());
            if (!captureFramebuffer->valid()) {
                return image;
            }
            GLFramebuffer::pushFramebuffer(captureFramebuffer.get());
        }

        if (GLFramebuffer::blitSupported() && !GLPlatform::instance()->isGLES()) {
            image = QImage(nativeSize.width(), nativeSize.height(), QImage::Format_ARGB32);
            GLTexture texture(GL_RGBA8, nativeSize.width(), nativeSize.height());
            GLFramebuffer target(&texture);
            target.blitFromFramebuffer(effects->mapToRenderTarget(QRectF(geometry)).toRect());
            // copy content from framebuffer into image
            texture.bind();
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                          static_cast<GLvoid *>(image.bits()));
            texture.unbind();
        } else {
            const QRect mappedRect = effects->mapToRenderTarget(QRectF(geometry)).toRect();
            const QSize fboSize = GLFramebuffer::currentFramebuffer()
                                ? GLFramebuffer::currentFramebuffer()->size()
                                : nativeSize;

            image = QImage(nativeSize.width(), nativeSize.height(), QImage::Format_ARGB32);

            // glReadPixels reads from current framebuffer (GL coords: Y=0 at bottom)
            // Convert screen Y (0 at top) to GL Y (0 at bottom): glY = fboHeight - screenY - height
            GLint glY = fboSize.height() - mappedRect.y() - mappedRect.height();

            glReadPixels(mappedRect.x(), glY, nativeSize.width(), nativeSize.height(), GL_RGBA,
                         GL_UNSIGNED_BYTE, static_cast<GLvoid *>(image.bits()));
        }
        if (captureFramebuffer) {
            GLFramebuffer::popFramebuffer();
        }
        convertFromGLImage(image, nativeSize.width(), nativeSize.height());
    }

    image.setDevicePixelRatio(devicePixelRatio);
    return image;
}

void ScreenShotEffect::grabPointerImage(QImage &snapshot, int xOffset, int yOffset) const
{
    if (effects->isCursorHidden()) {
        return;
    }

    const PlatformCursorImage cursor = effects->cursorImage();
    if (cursor.image().isNull()) {
        return;
    }

    QPainter painter(&snapshot);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(effects->cursorPos() - cursor.hotSpot() - QPoint(xOffset, yOffset), cursor.image());
}

bool ScreenShotEffect::isActive() const
{
    return (!m_windowScreenShots.isEmpty() || !m_areaScreenShots.isEmpty() || !m_screenScreenShots.isEmpty() || !m_windowScreenSizedShots.isEmpty())
        && !effects->isScreenLocked();
}

int ScreenShotEffect::requestedEffectChainPosition() const
{
    return 0;
}

void ScreenShotEffect::handleScreenAdded()
{
    cancelAreaScreenShots();
}

void ScreenShotEffect::handleScreenRemoved(EffectScreen *screen)
{
    cancelAreaScreenShots();

    for (int i = m_screenScreenShots.count() - 1; i >= 0; --i) {
        if (m_screenScreenShots[i].screen == screen) {
            m_screenScreenShots[i].promise.reportCanceled();
            m_screenScreenShots.removeAt(i);
        }
    }
}

void ScreenShotEffect::handleWindowClosed(EffectWindow *window)
{
    for (int i = m_windowScreenShots.count() - 1; i >= 0; --i) {
        if (m_windowScreenShots[i].window == window) {
            m_windowScreenShots[i].promise.reportCanceled();
            m_windowScreenShots.removeAt(i);
        }
    }

    for (int i = m_windowScreenSizedShots.count() - 1; i >= 0; --i) {
        if (m_windowScreenSizedShots[i].window == window) {
            m_windowScreenSizedShots[i].promise.reportCanceled();
            m_windowScreenSizedShots.removeAt(i);
        }
    }
}

void ScreenShotEffect::maskProtectedWindows(QImage &image, const QRect &captureGeometry)
{
    if (waylandServer()) {
        return;
    }

    const EffectWindowList windows = effects->stackingOrder();
    QPainter painter(&image);

    const QRect imageWindow(QPoint(0, 0), captureGeometry.size());
    painter.setWindow(imageWindow);

    const QImage lockIcon(QStringLiteral(":/resources/themes/icon-lock.svg"));

    bool globalProhibit = isGlobalProhibitScreenshot();
    if (globalProhibit) {
        painter.fillRect(imageWindow, QColor(0xD4, 0xD4, 0xD4));

        if (!lockIcon.isNull()) {
            QSize iconSize = lockIcon.size();
            const qreal maxIconRatio = 0.125;
            const qreal scale = qMin(imageWindow.width() / qreal(iconSize.width()),
                                     imageWindow.height() / qreal(iconSize.height())) * maxIconRatio;
            iconSize *= qBound(0.1, scale, 1.0);
            const QPoint iconPos(imageWindow.x() + (imageWindow.width() - iconSize.width()) / 2,
                                 imageWindow.y() + (imageWindow.height() - iconSize.height()) / 2);
            painter.drawImage(QRect(iconPos, iconSize), lockIcon, lockIcon.rect());
        }

        painter.end();
        return;
    }

    for (const EffectWindow *window : windows) {
        if (window->isMinimized() || window->isDeleted() || !window->isOnCurrentDesktop()) {
            continue;
        }
        qulonglong winId = window->windowId();
        if (!isProhibitScreenshot(winId)) {
            continue;
        }

        QRect winRect = window->frameGeometry().toRect();
        QRect clippedWinRect = winRect.intersected(captureGeometry);
        if (clippedWinRect.isEmpty()) {
            continue;
        }

        QRect imageWinRect = clippedWinRect.translated(-captureGeometry.topLeft());

        painter.fillRect(imageWinRect, QColor(0xD4, 0xD4, 0xD4));

        if (!lockIcon.isNull() && imageWinRect.width() >= 32 && imageWinRect.height() >= 32) {
            QSize iconSize = lockIcon.size();
            const qreal scale = qMin(imageWinRect.width() / qreal(iconSize.width()),
                                     imageWinRect.height() / qreal(iconSize.height())) * 0.5;
            iconSize *= qBound(0.1, scale, 1.0);
            QPoint windowCenter = imageWinRect.center();
            const QPoint iconPos(windowCenter.x() - iconSize.width() / 2,
                                 windowCenter.y() - iconSize.height() / 2);
            painter.drawImage(QRect(iconPos, iconSize), lockIcon, lockIcon.rect());
        }
    }
    painter.end();
}

bool ScreenShotEffect::isGlobalProhibitScreenshot()
{
    if (waylandServer()) {
        auto *ddeRestrict = waylandServer()->ddeRestrict();
        return !ddeRestrict || ddeRestrict->prohibitScreencast();
    }

    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (sessionBus.interface()->isServiceRegistered(ProhibitScreenshotDBusService)) {
        QDBusInterface remoteApp(ProhibitScreenshotDBusService, ProhibitScreenshotDBusPath,
                                 ProhibitScreenshotDBusInterface);
        remoteApp.setTimeout(500);
        QDBusReply<bool> reply = remoteApp.call("prohibited");
        if (reply.isValid() && reply.value()) {
            return true;
        }
    }
    return false;
}

bool ScreenShotEffect::isProhibitScreenshot(qulonglong winid)
{
    if (waylandServer()) {
        return effects->prohibitScreenshot(winid);
    }

    if (QX11Info::isPlatformX11()) {
        // TODO: 使用dbus可能会导致kwin超时卡死，应提出不依赖 ProhibitScreenshotDBusService 的方式
        QDBusConnection sessionBus = QDBusConnection::sessionBus();
        if (sessionBus.interface()->isServiceRegistered(ProhibitScreenshotDBusService)) {
            QDBusInterface remoteApp(ProhibitScreenshotDBusService, ProhibitScreenshotDBusPath, ProhibitScreenshotDBusInterface);
            remoteApp.setTimeout(1000);
            QDBusReply<bool> reply = remoteApp.call("queryProhibitWindowState", (int)winid);
            if (reply.isValid() && reply.value())
                return true;
        }
    }

    return false;
}

QImage ScreenShotEffect::getProhibitShotImage()
{
    QImage prohibitImage(":/resources/themes/icon-lock.svg");
    QImage prohibitShotImage = QImage(prohibitImage.size() * 2, QImage::Format_ARGB32);
    QPainter painter(&prohibitShotImage);
    prohibitShotImage.fill(QColor(0xD4, 0xD4, 0xD4));
    QPoint position((prohibitShotImage.width() - prohibitImage.width()) / 2, (prohibitShotImage.height() - prohibitImage.height()) / 2);
    painter.drawImage(position, prohibitImage);

    return prohibitShotImage;
}

} // namespace KWin

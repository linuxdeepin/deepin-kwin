/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "prohibitmask.h"

#include "effects.h"
#include "wayland/dderestrict_interface.h"
#include "wayland_server.h"
#include "workspace.h"

#include "kwingltexture.h"
#include "kwinglutils.h"

#include <QPainter>

namespace KWin
{

ProtectedRegions computeProtectedRegions(const QRect &viewport, qreal scale)
{
    ProtectedRegions result;

    if (!workspace() || !workspace()->hasProtectedWindow())
        return result;

    auto dde_restrict = waylandServer()->ddeRestrict();
    if (!dde_restrict)
        return result;

    auto protectedIds = dde_restrict->protectedWindowIdLists();
    if (protectedIds.isEmpty())
        return result;


    if (protectedIds.contains(-1)) {
        result.maskAll = true;
        return result;
    }
    if (effects) {
        auto *effs = static_cast<EffectsHandlerImpl *>(effects);
        if (effs->isEffectActived("multitaskview")
            || effs->isEffectActived("presentwindows")
            || effs->isEffectActived("highlightwindow")
            || effs->isEffectActived("splitscreen")) {
            result.maskAll = true;
            return result;
        }
    }

    const auto windows = workspace()->stackingOrder();
    for (auto *win : windows) {
        if (win->isMinimized() || win->isDeleted() || !win->isOnCurrentDesktop())
            continue;

        if (!static_cast<EffectsHandlerImpl *>(effects)->prohibitScreenshot(
                static_cast<qulonglong>(win->window())))
            continue;

        QRect winRect = win->frameGeometry().toRect();
        winRect = winRect.intersected(viewport);
        if (winRect.isEmpty())
            continue;

        QRect local = winRect.translated(-viewport.topLeft());
        local = QRect(local.topLeft() * scale, local.size() * scale);
        result.rects += local;
    }
    return result;
}



static std::unique_ptr<GLTexture> s_lockTexture = nullptr;

static void ensureLockTexture()
{
    if (s_lockTexture)
        return;
    QImage img(QStringLiteral(":/resources/themes/icon-lock.svg"));
    if (!img.isNull()) {
        s_lockTexture = std::make_unique<GLTexture>(img);
        s_lockTexture->setYInverted(false);
    }
}

static void drawLockIconGL(const QRect &rect, const QSize &fboSize, bool yInverted)
{
    ensureLockTexture();
    if (!s_lockTexture)
        return;
    s_lockTexture->setYInverted(yInverted);

    QSize iconSize = s_lockTexture->size();
    qreal s = qMin(rect.width() / qreal(iconSize.width()),
                   rect.height() / qreal(iconSize.height())) * 0.5;
    iconSize *= qBound(0.1, s, 1.0);
    QPoint iconPos(rect.center().x() - iconSize.width() / 2,
                   rect.center().y() - iconSize.height() / 2);
    QRectF iconRect(iconPos, iconSize);

    auto shader = ShaderManager::instance()->pushShader(ShaderTrait::MapTexture);
    QMatrix4x4 mvp;
    mvp.ortho(QRect(QPoint(0, 0), fboSize));
    mvp.translate(iconRect.left(), iconRect.top());
    shader->setUniform(GLShader::ModelViewProjectionMatrix, mvp);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    s_lockTexture->bind();
    s_lockTexture->render(QRect(QPoint(0, 0), iconSize), 1.0);
    s_lockTexture->unbind();
    glDisable(GL_BLEND);
    ShaderManager::instance()->popShader();
}

void applyProhibitMaskGL(const ProtectedRegions &regions, const QSize &fboSize, bool flipped, bool drawIcon)
{
    if (!regions.maskAll && regions.rects.isEmpty())
        return;


    GLboolean scissorWasEnabled = glIsEnabled(GL_SCISSOR_TEST);
    GLint oldScissor[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_SCISSOR_BOX, oldScissor);
    GLfloat oldClearColor[4] = {0, 0, 0, 1};
    glGetFloatv(GL_COLOR_CLEAR_VALUE, oldClearColor);

    glClearColor(0xD4 / 255.f, 0xD4 / 255.f, 0xD4 / 255.f, 1.f);

    if (regions.maskAll) {
        glDisable(GL_SCISSOR_TEST);
        glClear(GL_COLOR_BUFFER_BIT);
        if (drawIcon)
            drawLockIconGL(QRect(QPoint(0, 0), fboSize), fboSize, !flipped);
    } else {
        glEnable(GL_SCISSOR_TEST);
        for (const QRect &r : regions.rects) {
            const int scissorY = flipped ? r.y() : (fboSize.height() - r.bottom());
            glScissor(r.x(), scissorY, r.width(), r.height());
            glClear(GL_COLOR_BUFFER_BIT);
            if (drawIcon && r.width() >= 32 && r.height() >= 32)
                drawLockIconGL(r, fboSize, !flipped);
        }
    }

    glClearColor(oldClearColor[0], oldClearColor[1], oldClearColor[2], oldClearColor[3]);
    if (scissorWasEnabled) {
        glEnable(GL_SCISSOR_TEST);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }
    glScissor(oldScissor[0], oldScissor[1], oldScissor[2], oldScissor[3]);
}



static QImage s_lockIcon;

static void drawLockIconPainter(QPainter &painter, const QRect &rect, qreal maxIconRatio)
{
    if (s_lockIcon.isNull())
        s_lockIcon = QImage(QStringLiteral(":/resources/themes/icon-lock.svg"));
    if (s_lockIcon.isNull())
        return;

    QSize iconSize = s_lockIcon.size();
    qreal s = qMin(rect.width() / qreal(iconSize.width()),
                   rect.height() / qreal(iconSize.height())) * maxIconRatio;
    iconSize *= qBound(0.1, s, 1.0);
    QPoint iconPos(rect.center().x() - iconSize.width() / 2,
                   rect.center().y() - iconSize.height() / 2);
    painter.drawImage(QRect(iconPos, iconSize), s_lockIcon, s_lockIcon.rect());
}

void applyProhibitMaskCPU(QImage &image, const ProtectedRegions &regions)
{
    if (!regions.maskAll && regions.rects.isEmpty())
        return;

    QPainter p(&image);
    if (regions.maskAll) {
        p.fillRect(image.rect(), QColor(0xD4, 0xD4, 0xD4));
        drawLockIconPainter(p, image.rect(), 0.125);
    } else {
        for (const QRect &r : regions.rects) {
            p.fillRect(r, QColor(0xD4, 0xD4, 0xD4));
            if (r.width() >= 32 && r.height() >= 32)
                drawLockIconPainter(p, r, 0.5);
        }
    }
    p.end();
}

} // namespace KWin

// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "meetingprivacy.h"
#include "effects.h"
#include "workspace.h"
#include "core/output.h"
#include "main.h"

#include <kwineffects.h>
#include <kwinglutils.h>

#include <QImage>
#include <QPainter>
#include <QSvgRenderer>

namespace KWin
{

MeetingPrivacy::MeetingPrivacy()
    : Effect()
{
    connect(effects, &EffectsHandler::windowClosed, this, &MeetingPrivacy::slotWindowClosed);
}

MeetingPrivacy::~MeetingPrivacy()
{
}

bool MeetingPrivacy::supported()
{
    return effects->isOpenGLCompositing();
}

bool MeetingPrivacy::enabledByDefault()
{
    return supported();
}

bool MeetingPrivacy::isActive() const
{
    return workspace()->isMeetingPrivacyMode();
}

bool MeetingPrivacy::shouldProtect(EffectWindow *w) const
{
    if (!workspace()->isMeetingPrivacyMode()) {
        return false;
    }

    if (kwinApp()->operationMode() == Application::OperationModeX11) {
        return false;
    }

    if (w->isDesktop() || w->isOnScreenDisplay()
        || w->isMenu() || w->isDropdownMenu() || w->isPopupMenu()
        || w->isTooltip() || w->isComboBox()) {
        return false;
    }

    QUuid windowId = w->internalId();
    if (!workspace()->isWindowPrivacyProtected(windowId)) {
        return false;
    }

    Output *paintingScreen = workspace()->getCurrentPaintingScreen();
    if (!paintingScreen || paintingScreen->isInternal()) {
        return false;
    }

    return true;
}

std::shared_ptr<GLTexture> MeetingPrivacy::getMaskTexture(const QSize &size)
{
    auto it = m_maskTextureCache.find(size);
    if (it != m_maskTextureCache.end()) {
        return it.value();
    }

    QImage maskImage(size, QImage::Format_RGBA8888);
    maskImage.fill(Qt::white);

    QPainter painter(&maskImage);
    painter.setRenderHint(QPainter::Antialiasing);

    QSvgRenderer svgRenderer(QStringLiteral(":/resources/themes/icon-lock.svg"));
    if (svgRenderer.isValid()) {
        int iconSize = qMin(size.width(), size.height()) / 4;
        QRect iconRect((size.width() - iconSize) / 2, (size.height() - iconSize) / 2, iconSize, iconSize);
        svgRenderer.render(&painter, iconRect);
    }

    painter.end();

    auto texture = std::make_shared<GLTexture>(maskImage);
    texture->setFilter(GL_LINEAR);
    texture->setWrapMode(GL_CLAMP_TO_EDGE);

    m_maskTextureCache[size] = texture;
    return texture;
}

void MeetingPrivacy::drawWindow(EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data)
{
    if (!shouldProtect(w)) {
        effects->drawWindow(w, mask, region, data);
        return;
    }

    effects->drawWindow(w, mask, region, data);

    QRectF contentsRect = w->contentsRect();
    if (contentsRect.isEmpty()) {
        return;
    }

    auto maskTexture = getMaskTexture(contentsRect.size().toSize());
    if (!maskTexture) {
        return;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    ShaderManager *shaderManager = ShaderManager::instance();
    GLShader *shader = shaderManager->pushShader(ShaderTrait::MapTexture);

    QMatrix4x4 mvp = data.projectionMatrix();
    mvp.translate(contentsRect.x(), contentsRect.y());
    mvp.scale(contentsRect.width(), contentsRect.height());

    shader->setUniform(GLShader::ModelViewProjectionMatrix, mvp);
    shader->setUniform("sampler", 0);

    maskTexture->bind();

    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setAttribLayout(GLVertexBuffer::GLVertex2DLayout, 2, sizeof(GLVertex2D));

    auto map = static_cast<GLVertex2D *>(vbo->map(6 * sizeof(GLVertex2D)));

    map[0] = GLVertex2D{
        .position = QVector2D(0.0f, 0.0f),
        .texcoord = QVector2D(0.0f, 1.0f),
    };
    map[1] = GLVertex2D{
        .position = QVector2D(1.0f, 1.0f),
        .texcoord = QVector2D(1.0f, 0.0f),
    };
    map[2] = GLVertex2D{
        .position = QVector2D(0.0f, 1.0f),
        .texcoord = QVector2D(0.0f, 0.0f),
    };
    map[3] = GLVertex2D{
        .position = QVector2D(0.0f, 0.0f),
        .texcoord = QVector2D(0.0f, 1.0f),
    };
    map[4] = GLVertex2D{
        .position = QVector2D(1.0f, 0.0f),
        .texcoord = QVector2D(1.0f, 1.0f),
    };
    map[5] = GLVertex2D{
        .position = QVector2D(1.0f, 1.0f),
        .texcoord = QVector2D(1.0f, 0.0f),
    };

    vbo->unmap();
    vbo->render(GL_TRIANGLES);

    maskTexture->unbind();
    shaderManager->popShader();

    glDisable(GL_BLEND);
}

void MeetingPrivacy::slotWindowClosed(EffectWindow *w)
{
    m_maskTextureCache.remove(w->contentsRect().size().toSize());
}

}

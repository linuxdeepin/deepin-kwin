// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "scissorwindow.h"
#include "effects.h"

#include <deepin_kwineffects.h>
#include <deepin_kwinglplatform.h>
#include <deepin_kwinglutils.h>
#include <kwindowsystem.h>

#include <QFile>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QTextStream>

Q_DECLARE_METATYPE(QPainterPath)

static void ensureResources()
{
    // Must initialize resources manually because the effect is a static lib.
    Q_INIT_RESOURCE(scissor);
}

namespace KWin {

ScissorWindow::ScissorWindow() : Effect() {
    ensureResources();

    for (int i = 0; i < NCorners; ++i) {
        m_texMask[i] = nullptr;
    }

    m_maskShader = nullptr;

    reconfigure(ReconfigureAll);

    m_maskShader = ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture,
                                                                     QByteArray(),
                                                                     ":/effects/scissor/mask.frag"
                                                                  );

    m_filletOptimizeShader = ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture,
                                                                               QByteArray(),
                                                                               ":/effects/scissor/fillet.frag"
                                                                            );

    {
        for (int i = 0; i < KWindowSystem::windows().count(); ++i) {
            if (EffectWindow *win = effects->findWindow(KWindowSystem::windows().at(i)))
                windowAdded(win);
        }
        connect(effects, &EffectsHandler::windowAdded, this, &ScissorWindow::windowAdded);
        connect(effects, &EffectsHandler::windowDeleted, this, &ScissorWindow::windowDeleted);
    }
}

ScissorWindow::~ScissorWindow() {
    if (m_maskShader) delete m_maskShader;
    if (m_filletOptimizeShader) delete m_filletOptimizeShader;
    for (auto itr = m_texMaskMap.begin(); itr != m_texMaskMap.end(); itr++) {
        delete itr->second;
    }
    m_texMaskMap.clear();
    for (int i = 0; i < NCorners; ++i) {
        if (m_texMask[i]) delete m_texMask[i];
    }
}

void ScissorWindow::reconfigure(ReconfigureFlags flags) {
    Q_UNUSED(flags)
    setRoundedCornerRadius(18);
}

void ScissorWindow::setRoundedCornerRadius(int radius) {
    m_radius = radius;
    m_cornerSize = QSize(m_radius, m_radius);
    buildTextureMask();
}

void ScissorWindow::buildTextureMask() {
    for (int i = 0; i < NCorners; ++i) delete m_texMask[i];
    static int m = 4;
    QImage img(m_radius * 2 + m * 2, m_radius * 2 + m * 2, QImage::Format_ARGB32);
    img.fill(QColor(0, 0, 0, 0));
    QPainter p(&img);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 255));
    p.setRenderHint(QPainter::Antialiasing);
    p.drawEllipse(QRectF(m, m, m_radius * 2, m_radius * 2));
    p.end();
    m_texMask[TopLeft] = new GLTexture(img.copy(0, 0, m_radius + m, m_radius + m));
    m_texMask[TopRight] = new GLTexture(img.copy(m_radius + m, 0, m_radius + m, m_radius + m));
    m_texMask[BottomLeft] = new GLTexture(img.copy(0, m_radius + m, m_radius + m, m_radius + m));
    m_texMask[BottomRight] = new GLTexture(img.copy(m_radius + m, m_radius + m, m_radius + m, m_radius + m));
}

void ScissorWindow::prePaintWindow(EffectWindow *w, WindowPrePaintData &data,
                                   std::chrono::milliseconds time) {
    if (effects->hasActiveFullScreenEffect() ||
        w->isDesktop() || isMaximized(w)) {
        effects->prePaintWindow(w, data, time);
        return;
    }

    {
        QRect geo(w->frameGeometry());
        data.paint += geo;
        data.clip -= geo;
    }

    effects->prePaintWindow(w, data, time);
}

void ScissorWindow::drawWindow(EffectWindow *w, int mask, const QRegion& region, WindowPaintData &data) {
    if (w->isDesktop()
        || isMaximized(w)
        || (mask & (PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS)))
    {
        effects->drawWindow(w, mask, region, data);
        return;
    }

    const QVariant &data_clip_path = w->data(WindowClipPathRole);
    if (data_clip_path.isValid()) {
        const QPainterPath path = qvariant_cast<QPainterPath>(data_clip_path);
        static const int extraWindowFrame = 100;

        if (!m_clipMaskMap.count(w) || m_clipMaskMap[w].maskPath != path) {
            QImage maskImage(w->size() * 2, QImage::Format_RGBA8888);
            maskImage.fill(QColor(0, 0, 0, 0));
            QPainter pa(&maskImage);
            pa.setRenderHint(QPainter::Antialiasing);
            pa.scale(2, 2);
            pa.fillPath(path, QColor(255, 255, 255, 255));
            pa.strokePath(path, QPen(QColor(80, 80, 80, 60), 2));
            pa.end();

            m_clipMaskMap[w] = WindowMaskCache {
                .maskPath = path,
                .maskTexture = std::make_shared<GLTexture>(maskImage),
            };

            m_clipMaskMap[w].maskTexture->setFilter(GL_LINEAR);
            m_clipMaskMap[w].maskTexture->setWrapMode(GL_CLAMP_TO_EDGE);
        }

        const WindowMaskCache& cache = m_clipMaskMap[w];

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        QRect rect = w->rect();
        rect.adjust(-extraWindowFrame, -extraWindowFrame, extraWindowFrame, extraWindowFrame);

        {
            auto shader = m_maskShader;
            ShaderManager::instance()->pushShader(shader);
            shader->setUniform("sampler", 0);
            shader->setUniform("msk1", 2);
            auto old_shader = data.shader;
            data.shader = shader;

            std::shared_ptr<GLTexture> maskTexture = cache.maskTexture;
            glActiveTexture(GL_TEXTURE2); maskTexture->bind();

            glActiveTexture(GL_TEXTURE0);
            effects->drawWindow(w, mask, region, data);

            ShaderManager::instance()->popShader();
            data.shader = old_shader;

            glActiveTexture(GL_TEXTURE2);
            glActiveTexture(GL_TEXTURE0);

            maskTexture->unbind();
        }

        return;
    } else {
        float cornerRadius = 0.0f;
        const QVariant valueRadius = w->data(WindowRadiusRole);
        if (valueRadius.isValid()) {
            cornerRadius = w->data(WindowRadiusRole).toPointF().x();
        } else {
            if (!(w->isDesktop() || w->isDock())) {
                EffectsHandlerImpl *effs = static_cast<EffectsHandlerImpl *>(effects);
                auto e = effs->findEffect("splitscreen");
                if (e && e->isActive()) {
                    auto geom = effects->findScreen(w->screen()->name())->geometry();
                    if ((w->x() + data.xTranslation() == geom.x()) || (w->x() + data.xTranslation() + w->width() * data.xScale() == geom.x() + geom.width())) {
                    }
                    else
                        cornerRadius = 8;
                }
            }
        }
        if (cornerRadius < 2) {
            effects->drawWindow(w, mask, region, data);
            return;
        }

        if (0 == m_texMaskMap.count(cornerRadius))
        {
            QImage img(QSize(36,36), QImage::Format_RGBA8888);
            img.fill(QColor(0,0,0,0));
            QPainter painter(&img);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255,255,255,255));
            painter.setRenderHint(QPainter::Antialiasing);
            painter.drawEllipse(0,0,36,36);
            painter.end();

            m_texMaskMap[cornerRadius] = new GLTexture(img.copy(0, 0, 18, 18));
            m_texMaskMap[cornerRadius]->setFilter(GL_LINEAR);
            m_texMaskMap[cornerRadius]->setWrapMode(GL_CLAMP_TO_EDGE);
        }

        ShaderManager::instance()->pushShader(m_filletOptimizeShader);
        m_filletOptimizeShader->setUniform("typ1", 1);
        m_filletOptimizeShader->setUniform("sampler", 0);
        m_filletOptimizeShader->setUniform("msk1", 1);
        m_filletOptimizeShader->setUniform("k", QVector2D(w->width() / cornerRadius, w->height() / cornerRadius));
        if (w->hasDecoration()) {
            m_filletOptimizeShader->setUniform("typ2", 0);
        } else {
            m_filletOptimizeShader->setUniform("typ2", 1);
        }
        auto old_shader = data.shader;
        data.shader = m_filletOptimizeShader;

        glActiveTexture(GL_TEXTURE1);
        m_texMaskMap[cornerRadius]->bind();
        glActiveTexture(GL_TEXTURE0);
        effects->drawWindow(w, mask, region, data);
        ShaderManager::instance()->popShader();
        data.shader = old_shader;
        glActiveTexture(GL_TEXTURE1);
        m_texMaskMap[cornerRadius]->unbind();
        glActiveTexture(GL_TEXTURE0);
        return;
    }
}

bool ScissorWindow::enabledByDefault() { return supported(); }

bool ScissorWindow::supported() {
    return effects->isOpenGLCompositing() && GLRenderTarget::supported();
}

void ScissorWindow::windowAdded(EffectWindow *w) {
    if (!w->hasDecoration())
        return;
}

void ScissorWindow::windowDeleted(EffectWindow *w) {
    m_clipMaskMap.erase(w);
}

bool ScissorWindow::isMaximized(EffectWindow *w) {
    auto geom = effects->findScreen(w->screen()->name())->geometry();
    return (w->x() == geom.x() && w->width() == geom.width()) &&
           (w->y() == geom.y() && w->height() == geom.height());
}

bool ScissorWindow::isMaximized(EffectWindow *w, const PaintData& data)
{
    auto geom = effects->findScreen(w->screen()->name())->geometry();
    return (w->x() + data.xTranslation() == geom.x() && w->width() * data.xScale() == geom.width()) ||
            (w->y() + data.yTranslation() == geom.y() && w->height() * data.yScale() == geom.height());
}

}  // namespace KWin


/*
 * Copyright (C) 2020 ~ 2025 Uniontech Technology Co., Ltd.
 *
 * Author:     zjq <zhaojunqing@uniontech.com>
 *
 * Maintainer: zjq <zhaojunqing@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "scissorwindow.h"

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

namespace KWin {

ScissorWindow::ScissorWindow() : Effect(), m_shader(nullptr) {
    for (int i = 0; i < NCorners; ++i) {
        m_texMask[i] = nullptr;
    }

    reconfigure(ReconfigureAll);

    {
        QByteArray source;
        QTextStream stream(&source);

        GLPlatform *const gl = GLPlatform::instance();
        QByteArray varying, output, textureLookup;
        if (!gl->isGLES()) {
            const bool glsl_140 = gl->glslVersion() >= kVersionNumber(1, 40);
            if (glsl_140) stream << "#version 140\n";
            varying = glsl_140 ? QByteArrayLiteral("in")
                               : QByteArrayLiteral("varying");
            textureLookup = glsl_140 ? QByteArrayLiteral("texture")
                                     : QByteArrayLiteral("texture2D");
            output = glsl_140 ? QByteArrayLiteral("fragColor")
                              : QByteArrayLiteral("gl_FragColor");
        } else {
            const bool glsl_es_300 =
                GLPlatform::instance()->glslVersion() >= kVersionNumber(3, 0);
            if (glsl_es_300) stream << "#version 300 es\n";
            stream << "precision highp float;\n";
            varying = glsl_es_300 ? QByteArrayLiteral("in")
                                  : QByteArrayLiteral("varying");
            textureLookup = glsl_es_300 ? QByteArrayLiteral("texture")
                                        : QByteArrayLiteral("texture2D");
            output = glsl_es_300 ? QByteArrayLiteral("fragColor")
                                 : QByteArrayLiteral("gl_FragColor");
        }

        stream << "uniform float opacity;\n";
        stream << "uniform sampler2D img, msk, sha;\n";
        stream << varying << " vec2 texcoord0;\n";
        if (output != QByteArrayLiteral("gl_FragColor"))
            stream << "\nout vec4 " << output << ";\n";

        stream << "void main(){ \n";
        stream << "  vec4 c = " << textureLookup << "(img, texcoord0);\n";
        stream << "  vec4 s = " << textureLookup << "(sha, vec2(texcoord0.s,1-texcoord0.t));\n";
        stream << "  vec4 m = " << textureLookup << "(msk, texcoord0);\n";
        stream << "  vec4 clr = vec4(0);\n";
        stream << "  clr.rgb = s.rgb * opacity + c.rgb * (1.0 - s.a * opacity);\n";
        stream << "  clr.a = 1 - m.a;\n";
        stream << "  " << output << " = clr;\n";
        stream << "}\n";
        stream.flush();
        m_shader = ShaderManager::instance()->generateCustomShader(ShaderTrait::MapTexture, QByteArray(), source);
        int img = m_shader->uniformLocation("img");
        int sha = m_shader->uniformLocation("sha");
        int msk = m_shader->uniformLocation("msk");
        ShaderManager::instance()->pushShader(m_shader);
        m_shader->setUniform(img, 0);
        m_shader->setUniform(sha, 1);
        m_shader->setUniform(msk, 2);
        ShaderManager::instance()->popShader();
        for (int i = 0; i < KWindowSystem::windows().count(); ++i) {
            if (EffectWindow *win = effects->findWindow(KWindowSystem::windows().at(i)))
                windowAdded(win);
        }
        connect(effects, &EffectsHandler::windowAdded, this, &ScissorWindow::windowAdded);
    }
}

ScissorWindow::~ScissorWindow() {
    delete m_shader;
    for (int i = 0; i < NCorners; ++i) {
        delete m_texMask[i];
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
    if (!m_shader->isValid() || effects->hasActiveFullScreenEffect() ||
        w->isDesktop() || isMaximized(w)) {
        effects->prePaintWindow(w, data, time);
        return;
    }
    static int m = 4;
    QRect geo(w->frameGeometry());
    QRect rect[NCorners] = {
        QRect(geo.topLeft() - QPoint(m, m), m_cornerSize + QSize(m, m)),
        QRect(geo.topRight() - QPoint(m_radius, m), m_cornerSize + QSize(m, m)),
        QRect(geo.bottomRight() - QPoint(m_radius, m_radius),
              m_cornerSize + QSize(m, m)),
        QRect(geo.bottomLeft() - QPoint(m, m_radius),
              m_cornerSize + QSize(m, m))};
    for (int i = 0; i < NCorners; ++i) {
        data.paint += rect[i];
        data.clip -= rect[i];
    }
    QRegion border(QRegion(geo.adjusted(-1, -1, 1, 1)) - geo);
    border += QRegion(geo.x() + m_radius, geo.y(), geo.width() - m_radius * 2, 1);
    data.paint += border;
    data.clip -= border;
    effects->prePaintWindow(w, data, time);
}

void ScissorWindow::paintWindow(EffectWindow *w, int mask, QRegion region,
                                WindowPaintData &data) {
    const QVariant &data_clip_path = w->data(ScissorWindow::WindowClipPathRole);
    if (data_clip_path.isValid()) {
        if (!m_shader->isValid() || w->isDesktop() || isMaximized(w) ||
            (mask & (PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS))) {
            effects->paintWindow(w, mask, region, data);
            return;
        }
        static const int m = 100;
        const QVariant &data_clip_path = w->data(ScissorWindow::WindowClipPathRole);
        const QPainterPath path = qvariant_cast<QPainterPath>(data_clip_path);
        QImage image(w->width() + 2 * m, w->height() + 2 * m, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::black);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.translate(m, m);
        painter.drawPath(path);
        painter.end();
        QByteArray source0, source1;
        {
            QTextStream stream(&source0);
            stream << "#version 140\n";
            stream << "in vec4 position, texcoord;\n";
            stream << "out vec2 texcoord0;\n";
            stream << "uniform mat4 modelViewProjectionMatrix;\n";
            stream << "void main() { \n";
            stream << "  texcoord0 = texcoord.st;\n";
            stream << "  gl_Position = modelViewProjectionMatrix * position;\n";
            stream << "}\n";
            stream.flush();
        }
        {
            QTextStream stream(&source1);
            stream << "#version 140\n";
            stream << "uniform float w, h;\n";
            stream << "in vec2 texcoord0;\n";
            stream << "out vec4 fragColor;\n";
            stream << "uniform sampler2D sampler, msk1;\n";
            stream << "void main() { \n";
            stream << "  float Pi = 6.28318530718;\n";
            stream << "  float Directions = 60.0;\n";
            stream << "  float Quality = 50.0;\n";
            stream << "  float Size = 50.0;\n";
            stream << "  vec2 Radius = Size/vec2(1000, 1000);\n";
            stream << "  vec2 uv = texcoord0 - vec2(0, 0.01);\n";
            stream << "  vec4 Color = texture(sampler, uv);\n";
            stream << "  for(float d = 0.0; d < Pi; d += Pi / Directions){\n";
            stream << "    for(float i = 1.0 / Quality; i <= 1.0; i += 1.0 / Quality) {\n";
            stream << "      Color += texture(sampler, uv + vec2(cos(d), sin(d)) * Radius * i);\n";
            stream << "    }\n";
            stream << "  }\n";
            stream << "  Color /= Quality * Directions - 15.0;\n";
            stream << "  vec4 c1 = texture(sampler, texcoord0);\n";
            stream << "  vec4 c2 = Color;\n";
            stream << "  c2.rgb = vec3(0.1, 0.1, 0.1);\n";
            stream << "  fragColor = c2;\n";
            stream << "  fragColor.a *= (1 - c1.a) * 0.2;\n";
            stream << "}\n";
            stream.flush();
        }
        effects->addRepaintFull();
        auto shader = ShaderManager::instance()->generateCustomShader(ShaderTrait::MapTexture, source0, source1);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        ShaderManager::instance()->pushShader(shader);
        QRect rect = w->rect();
        rect.adjust(-m, -m, m, m);
        const int mvpMatrixLocation = m_shader->uniformLocation("modelViewProjectionMatrix");
        QMatrix4x4 mvp = data.screenProjectionMatrix();
        mvp.translate(w->x() - m, w->y() - m);
        shader->setUniform(mvpMatrixLocation, mvp);
        shader->setUniform("sampler", 0);
        shader->setUniform("w", w->width() + 2 * m);
        shader->setUniform("h", w->height() + 2 * m);
        GLTexture tex(image);
        glActiveTexture(GL_TEXTURE0);
        tex.bind();
        if (!w->windowClass().contains("launcher")) tex.render(region, rect);
        ShaderManager::instance()->popShader();
        glActiveTexture(GL_TEXTURE0);
        tex.unbind();
        {
            const QPainterPath path = qvariant_cast<QPainterPath>(data_clip_path);
            QImage img(w->size() * 2, QImage::Format_RGBA8888);
            img.fill(QColor(0, 0, 0, 0));
            QPainter pa(&img);
            pa.setRenderHint(QPainter::Antialiasing);
            pa.scale(2, 2);
            pa.fillPath(path, QColor(255, 255, 255, 255));
            pa.strokePath(path, QPen(QColor(80, 80, 80, 60), 2));
            pa.end();
            QByteArray source;
            {
                QTextStream stream(&source);
                GLPlatform *const gl = GLPlatform::instance();
                QByteArray varying, output, textureLookup;
                if (!gl->isGLES()) {
                    const bool glsl_140 = gl->glslVersion() >= kVersionNumber(1, 40);
                    if (glsl_140) stream << "#version 140\n";
                    varying = glsl_140 ? QByteArrayLiteral("in")
                                       : QByteArrayLiteral("varying");
                    textureLookup = glsl_140 ? QByteArrayLiteral("texture")
                                             : QByteArrayLiteral("texture2D");
                    output = glsl_140 ? QByteArrayLiteral("fragColor")
                                      : QByteArrayLiteral("gl_FragColor");
                } else {
                    const bool glsl_es_300 = GLPlatform::instance()->glslVersion() >= kVersionNumber(3, 0);
                    if (glsl_es_300) stream << "#version 300 es\n";
                    stream << "precision highp float;\n";
                    varying = glsl_es_300 ? QByteArrayLiteral("in")
                                          : QByteArrayLiteral("varying");
                    textureLookup = glsl_es_300
                                        ? QByteArrayLiteral("texture")
                                        : QByteArrayLiteral("texture2D");
                    output = glsl_es_300 ? QByteArrayLiteral("fragColor")
                                         : QByteArrayLiteral("gl_FragColor");
                }
                stream << "uniform sampler2D sampler, msk1;\n";
                stream << varying << " vec2 texcoord0;\n";
                if (output != QByteArrayLiteral("gl_FragColor"))
                    stream << "\nout vec4 " << output << ";\n";
                stream << "void main(){ \n";
                stream << "  vec4 c = " << textureLookup << "(sampler, texcoord0);\n";
                stream << "  vec4 m = " << textureLookup << "(msk1, texcoord0);\n";
                stream << "  c *= m.a;\n";
                stream << "  " << output << " = c;\n";
                stream << "}\n";
                stream.flush();
            }
            auto shader = ShaderManager::instance()->generateCustomShader(ShaderTrait::MapTexture, QByteArray(), source);
            ShaderManager::instance()->pushShader(shader);
            shader->setUniform("sampler", 0);
            shader->setUniform("msk1", 2);
            auto old_shader = data.shader;
            data.shader = shader;
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            GLTexture maskTexture(img);
            maskTexture.setFilter(GL_LINEAR);
            maskTexture.setWrapMode(GL_CLAMP_TO_EDGE);
            glActiveTexture(GL_TEXTURE2);
            maskTexture.bind();
            glActiveTexture(GL_TEXTURE0);
            effects->paintWindow(w, mask, region, data);
            ShaderManager::instance()->popShader();
            data.shader = old_shader;
            glActiveTexture(GL_TEXTURE2);
            maskTexture.unbind();
            glActiveTexture(GL_TEXTURE0);
            return;
        }
    } else {
        if (!m_shader->isValid() || w->isDesktop() || isMaximized(w) ||
            (mask & (PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS))) {
            effects->paintWindow(w, mask, region, data);
            return;
        }
        int cornerRadius = 0.0f;
        const QVariant valueRadius = w->data(WindowRadiusRole);
        if (valueRadius.isValid()) {
            cornerRadius = w->data(WindowRadiusRole).toPointF().x();
        }
        if (cornerRadius < 2) {
            effects->paintWindow(w, mask, region, data);
            return;
        }
        setRoundedCornerRadius(cornerRadius);
        QRect geo(w->frameGeometry());
        geo.setWidth(data.xScale() * geo.width());
        geo.setHeight(data.yScale() * geo.height());
        geo.translate(data.xTranslation(), data.yTranslation());
        static const int m = 4;
        QRect rect[NCorners] = {
            QRect(geo.topLeft() - QPoint(m, m), m_cornerSize + QSize(m, m)),
            QRect(geo.topRight() - QPoint(m_radius - 1, m), m_cornerSize + QSize(m, m)),
            QRect(geo.bottomRight() - QPoint(m_radius - 1, m_radius - 1), m_cornerSize + QSize(m, m)),
            QRect(geo.bottomLeft() - QPoint(m, m_radius - 1), m_cornerSize + QSize(m, m))};
        GLTexture *tex[NCorners];
        const QRect s(effects->virtualScreenGeometry());
        for (int i = 0; i < NCorners; ++i) {
            GLTexture *t = new GLTexture(GL_RGBA8, rect[i].size());
            t->bind();
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rect[i].x(), s.height() - rect[i].y() - rect[i].height(), rect[i].width(), rect[i].height());
            t->unbind();
            tex[i] = t;
        }
        effects->paintWindow(w, mask, region, data);
        effects->addRepaintFull();
        QImage img1, simg[4];
        const QVariant &imgV = w->data(ShadowMaskRole);
        if (imgV.isValid()) {
            img1 = w->data(ShadowMaskRole).value<QImage>();
        } else {
            img1 = QImage(300, 300, QImage::Format_ARGB32);
        }
        int off = w->data(ShadowOffsetRole).value<int>();
        int cpt = img1.width() / 2;
        int r1 = m_radius + m;
        simg[0] = img1.copy(off - m, off - m, r1, r1);
        simg[1] = img1.copy(cpt, off - m, r1, r1);
        simg[2] = img1.copy(cpt, cpt, r1, r1);
        simg[3] = img1.copy(off - m, cpt, r1, r1);
        GLTexture *shwT[4];
        shwT[0] = new GLTexture(simg[0]);
        shwT[1] = new GLTexture(simg[1]);
        shwT[2] = new GLTexture(simg[2]);
        shwT[3] = new GLTexture(simg[3]);

        // rounded the corners
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        const int mvpMatrixLocation = m_shader->uniformLocation("modelViewProjectionMatrix");
        ShaderManager *shaderMgr = ShaderManager::instance();
        shaderMgr->pushShader(m_shader);
        for (int i = 0; i < NCorners; ++i) {
            QMatrix4x4 mvp = data.screenProjectionMatrix();
            mvp.translate(rect[i].x(), rect[i].y());
            m_shader->setUniform(mvpMatrixLocation, mvp);
            m_shader->setUniform("opacity", float(data.opacity()));
            glActiveTexture(GL_TEXTURE2);
            m_texMask[3 - i]->bind();
            glActiveTexture(GL_TEXTURE1);
            shwT[i]->bind();
            glActiveTexture(GL_TEXTURE0);
            tex[i]->bind();
            tex[i]->render(region, rect[i]);
            tex[i]->unbind();
            shwT[i]->unbind();
            m_texMask[3 - i]->unbind();
        }
        shaderMgr->popShader();
        glDisable(GL_BLEND);
        for (int i = 0; i < NCorners; ++i) {
            delete shwT[i];
            delete tex[i];
        }
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        effects->addRepaintFull();
    }
}

bool ScissorWindow::enabledByDefault() { return supported(); }

bool ScissorWindow::supported() {
    return effects->isOpenGLCompositing() && GLRenderTarget::supported();
}

void ScissorWindow::windowAdded(EffectWindow *w) {
    if (!w->hasDecoration()) return;
}

bool ScissorWindow::isMaximized(EffectWindow *w) {
    auto geom = effects->findScreen(w->screen()->name())->geometry();
    return (w->x() == geom.x() && w->width() == geom.width()) ||
           (w->y() == geom.y() && w->height() == geom.height());
}

}  // namespace KWin

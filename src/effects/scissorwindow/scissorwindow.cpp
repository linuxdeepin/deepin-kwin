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

namespace KWin {

ScissorWindow::ScissorWindow() : Effect(), m_shader(nullptr) {
    for (int i = 0; i < NCorners; ++i) {
        m_texMask[i] = nullptr;
    }

    m_shader1 = nullptr;
    m_shader2 = nullptr;

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
    }

    {
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
            stream << "  float Quality = 80.0;\n";
            stream << "  float Size = 40.0;\n";
            stream << "  vec2 Radius = Size/vec2(800, 800);\n";
            stream << "  vec2 uv = texcoord0 - vec2(0, 0.012);\n";
            stream << "  vec4 Color = texture(sampler, uv);\n";
            stream << "  bool cond = (w > 300 && h > 300 && (uv.s > 0.8 || uv.s < 0.2 || uv.t > 0.8 || uv.t < 0.2)); \n";
            stream << "  cond = w < 200 || h < 200 || cond;\n";
            stream << "  if (cond) { \n";
            stream << "    for(float d = 0.0; d < Pi; d += Pi / Directions){\n";
            stream << "      for(float i = 1.0 / Quality; i <= 1.0; i += 1.0 / Quality) {\n";
            stream << "        Color += texture(sampler, uv + vec2(cos(d), sin(d)) * Radius * i);\n";
            stream << "      }\n";
            stream << "    }\n";
            stream << "  }\n";
            stream << "  Color /= Quality * Directions - 15.0;\n";
            stream << "  vec4 c1 = texture(sampler, texcoord0);\n";
            stream << "  vec4 c2 = Color;\n";
            stream << "  c2.rgb = vec3(0.05, 0.05, 0.05);\n";
            stream << "  fragColor = c2;\n";
            stream << "  fragColor.a *= (1 - c1.a) * 0.18;\n";
            stream << "}\n";
            stream.flush();
        }
        m_shader1 = ShaderManager::instance()->generateCustomShader(ShaderTrait::MapTexture, source0, source1);
    }

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
	    m_shader2 = ShaderManager::instance()->generateCustomShader(ShaderTrait::MapTexture, QByteArray(), source);
    }

    {
        QByteArray source;
        QTextStream stream(&source);
        GLPlatform *const gl = GLPlatform::instance();
        QByteArray varying, output, textureLookup;
        if (!gl->isGLES())
        {
            const bool glsl_140 = gl->glslVersion() >= kVersionNumber(1, 40);
            if (glsl_140)
                stream << "#version 140\n";
            varying = glsl_140 ? QByteArrayLiteral("in") : QByteArrayLiteral("varying");
            textureLookup = glsl_140 ? QByteArrayLiteral("texture") : QByteArrayLiteral("texture2D");
            output = glsl_140 ? QByteArrayLiteral("fragColor") : QByteArrayLiteral("gl_FragColor");
        }
        else
        {
            const bool glsl_es_300 = GLPlatform::instance()->glslVersion() >= kVersionNumber(3, 0);
            if (glsl_es_300)
                stream << "#version 300 es\n";
            stream << "precision highp float;\n";
            varying = glsl_es_300 ? QByteArrayLiteral("in") : QByteArrayLiteral("varying");
            textureLookup = glsl_es_300 ? QByteArrayLiteral("texture") : QByteArrayLiteral("texture2D");
            output = glsl_es_300 ? QByteArrayLiteral("fragColor") : QByteArrayLiteral("gl_FragColor");
        }
        stream << "uniform sampler2D sampler, msk1;\n";
        stream << "uniform sampler2D modulation;\n";
        stream << "uniform float saturation;\n";
        stream << "uniform vec2 k;\n";
        stream << "uniform int typ1, typ2;\n";
        stream << varying << " vec2 texcoord0;\n";
        if (output != QByteArrayLiteral("gl_FragColor"))
            stream << "\nout vec4 " << output << ";\n";
        stream << "void main(){ \n";
        stream << "  vec4 c = " << textureLookup << "(sampler, texcoord0);\n";
        stream << "  if (typ1 == 1) {\n";
        stream << "    if (typ2 == 1) {\n";
        stream << "      vec2 tc = texcoord0 * k;\n";
        stream << "      vec4 m0 = " << textureLookup << "(msk1, tc);\n";
        stream << "      tc = texcoord0 * k - vec2(0, k.t - 1.0);\n";
        stream << "      tc.t = 1.0 - tc.t;\n";
        stream << "      vec4 m1 = " << textureLookup << "(msk1, tc);\n";
        stream << "      tc = texcoord0 * k - vec2(k.s - 1.0, 0);\n";
        stream << "      tc.s = 1.0 - tc.s;\n";
        stream << "      vec4 m2 = " << textureLookup << "(msk1, tc);\n";
        stream << "      tc = 1.0 - ((texcoord0 - 1.0) * k + 1.0);\n";
        stream << "      vec4 m3 = " << textureLookup << "(msk1, tc);\n";
        stream << "      c *= (m0.a * m1.a * m2.a * m3.a);\n";
        stream << "    } else {\n";
        stream << "      if (texcoord0.t > 0.5) {\n";
        stream << "          vec2 tc = texcoord0 * k - vec2(0, k.t - 1.0);\n";
        stream << "          tc.t = 1.0 - tc.t;\n";
        stream << "          vec4 m1 = " << textureLookup << "(msk1, tc);\n";
        stream << "          tc = 1.0 - ((texcoord0 - 1.0) * k + 1.0);\n";
        stream << "          vec4 m3 = " << textureLookup << "(msk1, tc);\n";
        stream << "          c *= (m1.a * m3.a);\n";
        stream << "      }\n";
        stream << "    }\n";
        stream << "  }\n";
        stream << "  " << output << " = c;\n";
        stream << "}\n";
        stream.flush();
        m_shader3 = ShaderManager::instance()->generateCustomShader(ShaderTrait::MapTexture, QByteArray(), source);
    }

    {
        for (int i = 0; i < KWindowSystem::windows().count(); ++i) {
            if (EffectWindow *win = effects->findWindow(KWindowSystem::windows().at(i)))
                windowAdded(win);
        }
        connect(effects, &EffectsHandler::windowAdded, this, &ScissorWindow::windowAdded);
    }
}

ScissorWindow::~ScissorWindow() {
    if (m_shader) delete m_shader;
    if (m_shader1) delete m_shader1;
    if (m_shader2) delete m_shader2;
    if (m_shader3) delete m_shader3;
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
    if (!m_shader->isValid() || effects->hasActiveFullScreenEffect() ||
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

void ScissorWindow::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data) {
    const QVariant &data_clip_path = w->data(WindowClipPathRole);
    if (data_clip_path.isValid()) {
        if (!m_shader->isValid() || w->isDesktop() || isMaximized(w) ||
            (mask & (PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS))) {
            effects->paintWindow(w, mask, region, data);
            return;
        }
        {
            static const int m = 100;
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
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            auto shader = m_shader1;
            ShaderManager::instance()->pushShader(shader);
            QRect rect = w->rect();
            rect.adjust(-m, -m, m, m);
            const int mvpMatrixLocation = m_shader->uniformLocation("modelViewProjectionMatrix");
            QMatrix4x4 mvp = data.screenProjectionMatrix();
            mvp.translate(w->x() - m, w->y() - m);
            shader->setUniform(mvpMatrixLocation, mvp);
            shader->setUniform("sampler", 0);
            shader->setUniform("w", w->width());
            shader->setUniform("h", w->height());
            GLTexture tex(image);
            glActiveTexture(GL_TEXTURE0);
            tex.bind();
            if (!w->windowClass().contains("launcher")) tex.render(region, rect);
            ShaderManager::instance()->popShader();
            glActiveTexture(GL_TEXTURE0);
            tex.unbind();
        }
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
            auto shader = m_shader2;
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
            effects->addRepaintFull();
            return;
        }
    } else {
        if (!m_shader->isValid() || w->isDesktop() || isMaximized(w, data)) {
            effects->paintWindow(w, mask, region, data);
            return;
        }
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
            effects->paintWindow(w, mask, region, data);
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

        ShaderManager::instance()->pushShader(m_shader3);
        m_shader3->setUniform("typ1", 1);
        m_shader3->setUniform("sampler", 0);
        m_shader3->setUniform("msk1", 1);
        m_shader3->setUniform("k", QVector2D(w->width() / cornerRadius, w->height() / cornerRadius));
        if (w->hasDecoration()) {
            m_shader3->setUniform("typ2", 0);
        } else {
            m_shader3->setUniform("typ2", 1);
        }
        auto old_shader = data.shader;
        data.shader = m_shader3;

        glActiveTexture(GL_TEXTURE1);
        m_texMaskMap[cornerRadius]->bind();
        glActiveTexture(GL_TEXTURE0);
        effects->paintWindow(w, mask, region, data);
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

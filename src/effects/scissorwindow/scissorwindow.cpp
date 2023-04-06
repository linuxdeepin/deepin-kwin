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

    m_maskShader = nullptr;

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
	    m_maskShader = ShaderManager::instance()->generateCustomShader(ShaderTrait::MapTexture, QByteArray(), source);
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
        m_filletOptimizeShader = ShaderManager::instance()->generateCustomShader(ShaderTrait::MapTexture, QByteArray(), source);
    }

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
    if (m_shader) delete m_shader;
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

void ScissorWindow::drawWindow(EffectWindow *w, int mask, const QRegion& region, WindowPaintData &data) {
    if (!m_shader->isValid()
        || w->isDesktop()
        || isMaximized(w)
        || (mask & (PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS)))
    {
        effects->paintWindow(w, mask, region, data);
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
            effects->paintWindow(w, mask, region, data);

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

void ScissorWindow::windowDeleted(EffectWindow *w) {
    m_clipMaskMap.erase(w);

    const QVariant &data_clip_path = w->data(WindowClipPathRole);
    if (data_clip_path.isValid()) {
        // FIXME: The reason for redrawing is that there are false shadows in the window of the clip path, and the screen needs to be forcibly updated.
        effects->addRepaintFull();
    }
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

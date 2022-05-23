
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

#include <QPainter>
#include <QImage>
#include <QTextStream>
#include <deepin_kwinglutils.h>
#include <kwindowsystem.h>
#include <deepin_kwineffects.h>
#include <deepin_kwinglplatform.h>
#include "scissorwindow.h"

namespace KWin {
ScissorWindow::ScissorWindow() : Effect(), m_shader(nullptr)
{
    for (int i = 0; i < NCorners; ++i) {
        m_texMask[i] = nullptr;
    }

    reconfigure(ReconfigureAll);
    {
        QByteArray source;
        QTextStream stream(&source);
        GLPlatform * const gl = GLPlatform::instance();
        QByteArray varying, output, textureLookup;

        if (!gl->isGLES()) {
            const bool glsl_140 = gl->glslVersion() >= kVersionNumber(1, 40);
            if (glsl_140)
                stream << "#version 140\n";
            varying       = glsl_140 ? QByteArrayLiteral("in")         : QByteArrayLiteral("varying");
            textureLookup = glsl_140 ? QByteArrayLiteral("texture")    : QByteArrayLiteral("texture2D");
            output        = glsl_140 ? QByteArrayLiteral("fragColor")  : QByteArrayLiteral("gl_FragColor");
        } else {
            const bool glsl_es_300 = GLPlatform::instance()->glslVersion() >= kVersionNumber(3, 0);
            if (glsl_es_300)
                stream << "#version 300 es\n";
            stream << "precision highp float;\n";
            varying       = glsl_es_300 ? QByteArrayLiteral("in")         : QByteArrayLiteral("varying");
            textureLookup = glsl_es_300 ? QByteArrayLiteral("texture")    : QByteArrayLiteral("texture2D");
            output        = glsl_es_300 ? QByteArrayLiteral("fragColor")  : QByteArrayLiteral("gl_FragColor");
        }

        stream << "uniform sampler2D img, msk, sha;\n";
        stream <<  varying << " vec2 texcoord0;\n";
        if (output != QByteArrayLiteral("gl_FragColor"))
            stream << "\nout vec4 " << output << ";\n";

        stream << "void main(){ \n";
        stream << "  vec4 c = " << textureLookup << "(img, texcoord0);\n";
        stream << "  vec4 s = " << textureLookup << "(sha, vec2(texcoord0.s,1-texcoord0.t));\n";
        stream << "  vec4 m = " << textureLookup << "(msk, texcoord0);\n";
        stream << "  c.rgb = s.rgb + c.rgb*(1.0-s.a);\n";
        stream << "  c.a = m.a;\n";
        stream << "  " << output << " = c;\n";
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

        for (int i = 0; i < KWindowSystem::windows().count(); ++i)
            if (EffectWindow *win = effects->findWindow(KWindowSystem::windows().at(i)))
                windowAdded(win);
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

void ScissorWindow::buildTextureMask()
{
    for (int i = 0; i < NCorners; ++i)
        delete m_texMask[i];

    QImage img(m_radius*2, m_radius*2, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.fillRect(img.rect(), Qt::black);
    p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::black);
    p.setRenderHint(QPainter::Antialiasing);
    p.drawEllipse(img.rect());
    p.end();

    m_texMask[TopLeft] = new GLTexture(img.copy(0, 0, m_radius, m_radius));
    m_texMask[TopRight] = new GLTexture(img.copy(m_radius, 0, m_radius, m_radius));
    m_texMask[BottomLeft] = new GLTexture(img.copy(0, m_radius, m_radius, m_radius));
    m_texMask[BottomRight] = new GLTexture(img.copy(m_radius, m_radius, m_radius, m_radius));
}

void ScissorWindow::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds time)
{
    if (!m_shader->isValid() || effects->hasActiveFullScreenEffect() || w->isDesktop() || isMaximized(w))
    {
        effects->prePaintWindow(w, data, time);
        return;
    }

    QRect geo(w->frameGeometry());
    QRect rect[NCorners] = {
        QRect(geo.topLeft(), m_cornerSize),
        QRect(geo.topRight() - QPoint(m_radius-1, 0), m_cornerSize),
        QRect(geo.bottomLeft() - QPoint(0, m_radius-1), m_cornerSize),
        QRect(geo.bottomRight() - QPoint(m_radius-1, m_radius-1), m_cornerSize)
    };
    for (int i = 0; i < NCorners; ++i) {
        data.paint += rect[i];
        data.clip -= rect[i];
    }

    QRegion border(QRegion(geo.adjusted(-1, -1, 1, 1)) - geo);
    data.paint += border;
    data.clip -= border;
    effects->prePaintWindow(w, data, time);
}

void ScissorWindow::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    if (!m_shader->isValid() || w->isDesktop() || isMaximized(w)
            || (w->windowClass().contains("dde-dock") && w->isNormalWindow())
            || (mask & (PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS))
            )
    {
        effects->paintWindow(w, mask, region, data);
        return;
    }

    int cornerRadius = 18;
    const QVariant valueRadius1 = w->data(WindowRadiusRole);
    if (valueRadius1.isValid() && w->isPopupMenu()) {
        cornerRadius = w->data(WindowRadiusRole).toPointF().x();
    }
    setRoundedCornerRadius(cornerRadius);

    //map the corners
    QRect geo(w->frameGeometry());
    QRect rect[NCorners] = {
        QRect(geo.topLeft(), m_cornerSize),
        QRect(geo.topRight()-QPoint(m_radius-1, 0), m_cornerSize),
        QRect(geo.bottomRight()-QPoint(m_radius-1, m_radius-1), m_cornerSize),
        QRect(geo.bottomLeft()-QPoint(0, m_radius-1), m_cornerSize)
    };

    GLTexture* tex[NCorners];
    const QRect s(effects->virtualScreenGeometry());
    for (int i = 0; i < NCorners; ++i) {
        GLTexture *t = new GLTexture(GL_RGBA8, rect[i].size());
        t->bind();
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
			    rect[i].x(), s.height() - rect[i].y() - rect[i].height(), rect[i].width(), rect[i].height());
        t->unbind();
	    tex[i] = t;
    }
    effects->paintWindow(w, mask, region, data);

    QImage img1, simg[4];
    const QVariant &imgV = w->data(ShadowMaskRole);
    if (imgV.isValid()) {
        img1 = w->data(ShadowMaskRole).value<QImage>();
    } else {
        img1 = QImage(300,300,QImage::Format_ARGB32);
    }

    int off = w->data(ShadowOffsetRole).value<int>();
    int cpt = img1.width() / 2;
    simg[0] = img1.copy(off,off, m_radius, m_radius);
    simg[1] = img1.copy(cpt,off, m_radius, m_radius);
    simg[2] = img1.copy(cpt, cpt, m_radius, m_radius);
    simg[3] = img1.copy(off, cpt, m_radius, m_radius);

    GLTexture *shwT[4];
    shwT[0] = new GLTexture(simg[0]);
    shwT[1] = new GLTexture(simg[1]);
    shwT[2] = new GLTexture(simg[2]);
    shwT[3] = new GLTexture(simg[3]);

    //rounded the corners
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    const int mvpMatrixLocation = m_shader->uniformLocation("modelViewProjectionMatrix");

    ShaderManager *shaderMgr = ShaderManager::instance();
    shaderMgr->pushShader(m_shader);
    for (int i = 0; i < NCorners; ++i) {
        QMatrix4x4 mvp = data.screenProjectionMatrix();
        mvp.translate(rect[i].x(), rect[i].y());
        m_shader->setUniform(mvpMatrixLocation, mvp);
        glActiveTexture(GL_TEXTURE2); m_texMask[3-i]->bind();
        glActiveTexture(GL_TEXTURE1); shwT[i]->bind();
        glActiveTexture(GL_TEXTURE0); tex[i]->bind();
        tex[i]->render(region, rect[i]);
        tex[i]->unbind(); shwT[i]->unbind(); m_texMask[3-i]->unbind();
    }
    shaderMgr->popShader();
    glDisable(GL_BLEND);
    delete shwT[0]; delete shwT[1]; delete shwT[2]; delete shwT[3];
    for (int i = 0; i < NCorners; ++i) delete tex[i];
}

bool ScissorWindow::enabledByDefault() {
    return supported();
}

bool ScissorWindow::supported() {
    return effects->isOpenGLCompositing() && GLRenderTarget::supported();
}

void ScissorWindow::windowAdded(EffectWindow *w)
{
    if (!w->hasDecoration())
        return;
}

bool ScissorWindow::isMaximized(EffectWindow *w)
{
    auto geom = effects->findScreen(w->screen()->name())->geometry();
    return (w->x() == geom.x() && w->width() == geom.width()) ||
            (w->y() == geom.y() && w->height() == geom.height());
}
}

// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "black-screen.h"

#include <QGuiApplication>
#include <QMatrix4x4>
#include <DWayland/Server/surface_interface.h>
#include <DWayland/Server/display.h>

bool BlackScreenEffect::supported()
{
    bool supported = KWin::effects->isOpenGLCompositing();
    return supported;
}

BlackScreenEffect::BlackScreenEffect(QObject *, const QVariantList &)
    : Effect()
{
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/BlackScreen"), this, QDBusConnection::ExportScriptableContents);
}

BlackScreenEffect::~BlackScreenEffect()
{
    QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/BlackScreen"));
}
void BlackScreenEffect::drawWindow(KWin::EffectWindow *w, int mask, const QRegion &_region, KWin::WindowPaintData &data)
{
    QRegion region = _region;
    if (!m_activated) {
        effects->drawWindow(w, mask, region, data);
        return;
    }
    const QRect screen = GLRenderTarget::virtualScreenGeometry();
    QColor color(1, 1, 0);
    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setUseColor(true);
    ShaderBinder binder(ShaderTrait::UniformColor);
    binder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, data.screenProjectionMatrix());
    vbo->setColor(color);

    QVector<float> verts;
    verts.reserve(region.rects().count() * 12);

    verts << screen.x() + screen.width() << screen.y();
    verts << screen.x() << screen.y();
    verts << screen.x() << screen.y() + screen.height();
    verts << screen.x() << screen.y() + screen.height();
    verts << screen.x() + screen.width() << screen.y() + screen.height();
    verts << screen.x() + screen.width() << screen.y();

    vbo->setData(verts.count() / 2, 2, verts.data(), NULL);
    vbo->render(GL_TRIANGLES);
}

bool BlackScreenEffect::isActive() const
{
    return m_activated;
}

void BlackScreenEffect::setActive(bool active)
{
    if (m_activated == active) {
        return;
    }

    m_activated = active;

    if (m_activated) {
        effects->hideCursor();
    } else {
        effects->showCursor();
    }
    effects->addRepaintFull();
}

bool BlackScreenEffect::getActive()
{
    return m_activated;
}

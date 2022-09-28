/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "scene_qpainter_drm_backend.h"
#include "drm_backend.h"
#include "drm_output.h"
#include "logind.h"

namespace KWin
{

DrmQPainterBackend::DrmQPainterBackend(DrmBackend *backend)
    : QObject()
    , QPainterBackend()
    , m_backend(backend)
{
    const auto outputs = m_backend->drmOutputs();
    for (auto output: outputs) {
        initOutput(output);
    }
    connect(m_backend, &DrmBackend::outputAdded, this, &DrmQPainterBackend::initOutput);
    connect(m_backend, &DrmBackend::outputRemoved, this,
        [this] (DrmOutput *o) {
            auto it = std::find_if(m_outputs.begin(), m_outputs.end(),
                [o] (const Output &output) {
                    return output.output == o;
                }
            );
            if (it == m_outputs.end()) {
                return;
            }
            delete (*it).buffer[0];
            delete (*it).buffer[1];
            m_outputs.erase(it);
        }
    );
}

DrmQPainterBackend::~DrmQPainterBackend()
{
    for (auto it = m_outputs.begin(); it != m_outputs.end(); ++it) {
        delete (*it).buffer[0];
        delete (*it).buffer[1];
    }
}

void DrmQPainterBackend::initOutput(DrmOutput *output)
{
    Output o;
    auto initBuffer = [&o, output, this] (int index) {
        o.buffer[index] = m_backend->createBuffer(output->pixelSize());
        if (o.buffer[index]->map()) {
            o.buffer[index]->image()->fill(Qt::black);
        }
    };
    connect(output, &DrmOutput::modeChanged, this,
        [output, this] {
            auto it = std::find_if(m_outputs.begin(), m_outputs.end(),
                [output] (const auto &o) {
                    return o.output == output;
                }
            );
            if (it == m_outputs.end()) {
                return;
            }
            delete (*it).buffer[0];
            delete (*it).buffer[1];
            auto initBuffer = [it, output, this] (int index) {
                it->buffer[index] = m_backend->createBuffer(output->pixelSize());
                if (it->buffer[index]->map()) {
                    it->buffer[index]->image()->fill(Qt::black);
                }
            };
            initBuffer(0);
            initBuffer(1);
        }
    );
    initBuffer(0);
    initBuffer(1);
    o.output = output;
    m_outputs << o;
}

QImage *DrmQPainterBackend::buffer()
{
    return bufferForScreen(0);
}

QImage *DrmQPainterBackend::bufferForScreen(int screenId)
{
    const Output &o = m_outputs.at(screenId);
    return o.buffer[o.index]->image();
}

bool DrmQPainterBackend::needsFullRepaint() const
{
    return true;
}

void DrmQPainterBackend::prepareRenderingFrame()
{
    for (auto it = m_outputs.begin(); it != m_outputs.end(); ++it) {
        (*it).index = ((*it).index + 1) % 2;
    }
}

void DrmQPainterBackend::present(int mask, const QRegion &damage)
{
    Q_UNUSED(mask)
    Q_UNUSED(damage)
    if (!LogindIntegration::self()->isActiveSession()) {
        return;
    }
    for (auto it = m_outputs.begin(); it != m_outputs.end(); ++it) {
        const Output &o = *it;
        m_backend->present(o.buffer[o.index], o.output);
    }
}

bool DrmQPainterBackend::usesOverlayWindow() const
{
    return false;
}

bool DrmQPainterBackend::perScreenRendering() const
{
    return true;
}

}

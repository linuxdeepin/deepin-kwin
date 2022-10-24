// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-License-Identifier: LGPL-2.0-or-later

#include "dmabuftexture.h"

#include "kwineglimagetexture.h"
#include "kwinglutils.h"

namespace KWin
{

DmaBufTexture::DmaBufTexture(KWin::GLTexture *texture)
    : m_texture(texture)
    , m_framebuffer(new KWin::GLRenderTarget(*m_texture))
{
}

DmaBufTexture::~DmaBufTexture() = default;

KWin::GLRenderTarget *DmaBufTexture::framebuffer() const
{
    return m_framebuffer.data();
}

} // namespace KWin

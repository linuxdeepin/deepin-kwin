/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

namespace KWin
{

enum class TextureFilterMode { Nearest,
                               Linear,
                               Mipmap };

struct TextureFilterPlan
{
    TextureFilterMode minFilter;
    TextureFilterMode magFilter;
    bool generateMipmap = false;
};

/**
 * @brief Chooses texture filter modes for the given conditions.
 * @param smooth Whether smooth filtering is requested.
 * @param hasGLContext Whether a GL context is available.
 * @return The filter plan to apply.
 */
inline TextureFilterPlan textureFilterPlan(bool smooth, bool hasGLContext)
{
    if (!smooth) {
        return {TextureFilterMode::Nearest, TextureFilterMode::Nearest, false};
    }
    if (!hasGLContext) {
        return {TextureFilterMode::Linear, TextureFilterMode::Linear, false};
    }
    return {TextureFilterMode::Mipmap, TextureFilterMode::Linear, true};
}

}

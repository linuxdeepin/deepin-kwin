/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QImage>
#include <QSize>
#include <QString>

namespace KWin
{

/**
 * Decodes the image at @p path into a copy no larger than @p targetSize.
 *
 * The size is the one the caller is going to draw at, so the decoder is asked for
 * that size directly instead of materialising the full picture and throwing most of
 * it away afterwards: a 4K wallpaper shown as a 206x129 bar thumbnail used to cost a
 * full 4K decode plus a smooth downscale on the compositor thread.
 *
 * Aspect ratio is kept. Raster sources smaller than @p targetSize are returned
 * unchanged, because upscaling belongs to the renderer, not to the decoder; a vector
 * source is rendered at the requested size, since that is what it is for.
 *
 * Decoding is *not* clamped to a scale step: the JPEG decoder answers with a size of
 * its own choosing (a fraction of the source), which is why the result is settled on
 * the requested size afterwards. That last scale only ever shrinks a small image.
 */
QImage decodeImage(const QString &path, const QSize &targetSize);

}

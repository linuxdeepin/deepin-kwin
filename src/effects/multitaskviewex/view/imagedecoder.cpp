/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "imagedecoder.h"

#include <QImageReader>
#include <QPainter>
#include <QSvgRenderer>

namespace KWin
{

namespace
{

QImage readRaster(const QString &path, const QSize &targetSize)
{
    QImageReader reader(path);
    reader.setDecideFormatFromContent(true);

    const QSize sourceSize = reader.size();
    if (sourceSize.isValid() && !sourceSize.isEmpty() && targetSize.isValid()) {
        const QSize wanted = sourceSize.scaled(targetSize, Qt::KeepAspectRatio);
        if (wanted.width() < sourceSize.width() || wanted.height() < sourceSize.height()) {
            reader.setScaledSize(wanted);
        }
    }

    return reader.read();
}

QImage readSvg(const QString &path, const QSize &targetSize)
{
    QSvgRenderer renderer(path);
    if (!renderer.isValid()) {
        return QImage();
    }
    QSize want = targetSize.isValid() ? targetSize : QSize(32, 32);
    if (!renderer.defaultSize().isEmpty()) {
        want = renderer.defaultSize().scaled(want, Qt::KeepAspectRatio);
    }
    QImage img(want, QImage::Format_RGBA8888);
    img.fill(Qt::transparent);
    QPainter painter(&img);
    renderer.render(&painter, QRectF(0, 0, want.width(), want.height()));
    painter.end();
    return img;
}

} // namespace

QImage decodeImage(const QString &path, const QSize &targetSize)
{
    if (path.endsWith(QStringLiteral(".svg"))) {
        return readSvg(path, targetSize);
    }

    QImage img = readRaster(path, targetSize);
    if (img.isNull() || !targetSize.isValid()) {
        return img;
    }

    const QSize wanted = img.size().scaled(targetSize, Qt::KeepAspectRatio);
    if (wanted.isValid() && !wanted.isEmpty() && (wanted.width() < img.width() || wanted.height() < img.height())) {
        img = img.scaled(wanted, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return img;
}

}

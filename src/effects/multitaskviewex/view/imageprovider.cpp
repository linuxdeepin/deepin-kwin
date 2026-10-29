/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "imageprovider.h"
#include "glpaintrenderer.h"

#include <QFileInfo>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QScreen>
#include <QSvgRenderer>
#include <QThreadPool>
#include <QUrl>
#include <QtConcurrent>
#include <QtMath>

namespace KWin
{

ImageProvider::ImageProvider(QObject *parent)
    : QObject(parent)
{
}

static QString stripPrefix(const QString &path)
{
    QString p = path;
    if (p.startsWith(QStringLiteral("file://"))) {
        p = p.mid(7);
    }
    return p;
}

static QString textureKey(const QString &source, const QSize &size)
{
    return QStringLiteral("%1@%2x%3").arg(stripPrefix(source)).arg(size.width()).arg(size.height());
}

static QImage loadSourceImage(const QString &path, const QSize &targetSize)
{
    QImage img;
    if (path.endsWith(QStringLiteral(".svg"))) {
        QSvgRenderer renderer(path);
        if (renderer.isValid()) {
            QSize want = targetSize.isValid() ? targetSize : QSize(32, 32);
            if (!renderer.defaultSize().isEmpty()) {
                want = renderer.defaultSize().scaled(want, Qt::KeepAspectRatio);
            }
            img = QImage(want, QImage::Format_RGBA8888);
            img.fill(Qt::transparent);
            QPainter painter(&img);
            renderer.render(&painter, QRectF(0, 0, want.width(), want.height()));
            painter.end();
        }
    } else {
        img = QImage(path);
        if (img.isNull()) {
            QImageReader reader(path);
            reader.setDecideFormatFromContent(true);
            reader.read(&img);
        }
        if (img.isNull()) {
            const QString real = QFileInfo(path).symLinkTarget();
            if (!real.isEmpty() && real != path) {
                img = QImage(real);
                if (img.isNull()) {
                    QImageReader reader2(real);
                    reader2.setDecideFormatFromContent(true);
                    reader2.read(&img);
                }
            }
        }
    }
    return img;
}

ImageProvider::TextureInfo ImageProvider::textureFor(const QString &source, const QSize &targetSize)
{
    return textureForImpl(source, targetSize, true, true);
}

ImageProvider::TextureInfo ImageProvider::textureForReady(const QString &source, const QSize &targetSize, bool smooth)
{
    return textureForImpl(source, targetSize, false, smooth);
}

QString ImageProvider::textureKeyFor(const QString &source, const QSize &size)
{
    return textureKey(source, size);
}

ImageProvider::TextureInfo ImageProvider::textureForImpl(const QString &source, const QSize &targetSize,
                                                         bool allowSyncLoad, bool smooth)
{
    const QString path = stripPrefix(source);
    const QString key = textureKey(path, targetSize);
    TextureInfo info;
    info.source = path;

    auto it = m_resources.find(key);
    if (it == m_resources.end() || it.value().image.isNull()) {
        const auto scaledCached = m_decodedScaled.constFind(key);
        const auto decoded = m_decoded.constFind(path);
        QImage img;
        if (scaledCached != m_decodedScaled.constEnd()) {
            img = scaledCached.value();
        } else if (decoded != m_decoded.constEnd()) {
            img = decoded.value();
        } else if (allowSyncLoad) {
            img = loadSourceImage(path, targetSize);
        } else {
            return info;
        }
        if (img.isNull()) {
            return info;
        }
        QSize need = img.size();
        if (targetSize.isValid() && targetSize.width() > 0 && targetSize.height() > 0) {
            const QSize scaled = img.size().scaled(targetSize, Qt::KeepAspectRatio);
            if (scaled.width() < img.width() || scaled.height() < img.height()) {
                need = scaled;
            }
        }
        if (need.isEmpty()) {
            need = img.size();
        }
        const Qt::TransformationMode mode = smooth ? Qt::SmoothTransformation : Qt::FastTransformation;
        const QImage scaledImg = need == img.size() ? img : img.scaled(need, Qt::KeepAspectRatio, mode);
        if (it == m_resources.end()) {
            Resource res;
            res.source = path;
            res.image = scaledImg;
            res.imageSize = res.image.size();
            res.textureSize = res.image.size();
            it = m_resources.insert(key, std::move(res));
        } else {
            it.value().source = path;
            it.value().image = scaledImg;
            it.value().imageSize = it.value().image.size();
            it.value().textureSize = it.value().image.size();
            it.value().dirty = true;
        }
    }
    if (!m_renderer) {
        m_renderer = &GLPaintRenderer::instance();
    }
    ensureTexture(&it.value());

    info.texture = it.value().texture.get();
    info.imageSize = it.value().imageSize;
    info.textureSize = it.value().textureSize;
    return info;
}

void ImageProvider::acquireTexture(const QString &source, const QSize &targetSize)
{
    const QString key = textureKey(source, targetSize);
    auto it = m_resources.find(key);
    if (it == m_resources.end()) {
        Resource res;
        res.source = stripPrefix(source);
        it = m_resources.insert(key, std::move(res));
    }
    it.value().useCount++;
}

void ImageProvider::releaseTexture(const QString &source, const QSize &targetSize)
{
    const QString key = textureKey(source, targetSize);
    auto it = m_resources.find(key);
    if (it == m_resources.end()) {
        return;
    }
    if (it.value().useCount > 0) {
        it.value().useCount--;
    }
}

void ImageProvider::pin(const QString &source, const QSize &targetSize)
{
    const QString key = textureKey(source, targetSize);
    auto it = m_resources.find(key);
    if (it == m_resources.end()) {
        Resource res;
        res.source = stripPrefix(source);
        res.pinned = true;
        m_resources.insert(key, std::move(res));
    } else {
        it.value().pinned = true;
    }
}

void ImageProvider::unpin(const QString &source, const QSize &targetSize)
{
    const QString key = textureKey(source, targetSize);
    auto it = m_resources.find(key);
    if (it != m_resources.end()) {
        it.value().pinned = false;
    }
}

ImageProvider::TextureInfo ImageProvider::publishImage(Resource &res)
{
    TextureInfo info;
    info.source = res.source;
    if (res.textureSize != res.imageSize) {
        res.textureSize = res.imageSize;
        res.dirty = true;
    }
    if (!m_renderer) {
        m_renderer = &GLPaintRenderer::instance();
    }
    ensureTexture(&res);

    info.texture = res.texture.get();
    info.imageSize = res.imageSize;
    info.textureSize = res.textureSize;
    return info;
}

ImageProvider::TextureInfo ImageProvider::textTexture(const QString &text, const QFont &font, const QColor &color)
{
    if (text.isEmpty()) {
        return TextureInfo{};
    }
    qreal dpi = 96.0;
    if (QScreen *scr = QGuiApplication::primaryScreen()) {
        dpi = scr->logicalDotsPerInch();
    }
    const QString key = QStringLiteral("text:%1|%2|%3|%4|%5|%6")
                            .arg(text, font.family())
                            .arg(font.pointSize())
                            .arg(int(font.weight()))
                            .arg(color.name())
                            .arg(dpi);
    auto it = m_resources.find(key);
    if (it == m_resources.end()) {
        const QFontMetricsF fm(font);
        const QRectF br = fm.boundingRect(QRectF(0, 0, 4096, 256), Qt::AlignLeft | Qt::AlignTop, text);
        const QSize size(qCeil(br.width()) + 2, qCeil(fm.height()));
        if (size.isEmpty()) {
            return TextureInfo{};
        }
        QImage img(size, QImage::Format_RGBA8888);
        img.setDotsPerMeterX(qRound(dpi / 0.0254));
        img.setDotsPerMeterY(qRound(dpi / 0.0254));
        img.fill(Qt::transparent);
        QPainter p(&img);
        p.setFont(font);
        p.setPen(color);
        p.drawText(QRect(QPoint(0, 0), size), Qt::AlignLeft | Qt::AlignVCenter, text);
        p.end();
        Resource res;
        res.source = key;
        res.imageSize = img.size();
        res.image = img;
        it = m_resources.insert(key, std::move(res));
    }
    return publishImage(it.value());
}

void ImageProvider::invalidate()
{
    for (auto &res : m_resources) {
        res.texture.reset();
        res.dirty = true;
    }
}

void ImageProvider::trimUnused()
{
    for (auto it = m_resources.begin(); it != m_resources.end();) {
        if (it.value().useCount > 0 || it.value().pinned) {
            ++it;
            continue;
        }
        it = m_resources.erase(it);
    }
    QSet<QString> inUse;
    for (auto it = m_resources.constBegin(); it != m_resources.constEnd(); ++it) {
        inUse.insert(it.value().source);
    }
    for (auto it = m_decoded.begin(); it != m_decoded.end();) {
        if (inUse.contains(it.key())) {
            ++it;
        } else {
            it = m_decoded.erase(it);
        }
    }
    for (auto it = m_decodedScaled.begin(); it != m_decodedScaled.end();) {
        const int sep = it.key().lastIndexOf(QLatin1Char('@'));
        const QString source = sep < 0 ? it.key() : it.key().left(sep);
        if (inUse.contains(source)) {
            ++it;
        } else {
            it = m_decodedScaled.erase(it);
        }
    }
}

void ImageProvider::ensureTexture(Resource *res)
{
    if (!res->dirty) {
        return;
    }
    if (!m_renderer) {
        return;
    }
    res->texture.reset();
    res->texture = m_renderer->uploadImage(res->image, !res->source.endsWith(QStringLiteral(".svg")));
    if (!res->texture) {
        res->textureSize = QSize();
    }
    res->dirty = false;
}

void ImageProvider::predecode(const QStringList &sources)
{
    for (const QString &raw : sources) {
        const QString path = stripPrefix(raw);
        if (path.isEmpty() || m_decoded.contains(path) || m_predecoding.contains(path)) {
            continue;
        }
        m_predecoding.append(path);
        QFuture<void> f = QtConcurrent::run(QThreadPool::globalInstance(), [this, path]() {
            const QImage img = loadSourceImage(path, QSize());
            QMetaObject::invokeMethod(
                this,
                [this, path, img]() {
                    m_predecoding.removeAll(path);
                    if (!img.isNull()) {
                        m_decoded.insert(path, img);
                        Q_EMIT predecoded(path);
                    }
                },
                Qt::QueuedConnection);
        });
        m_predecodeSync.addFuture(f);
    }
}

void ImageProvider::predecode(const QStringList &sources, const QSize &targetSize)
{
    for (const QString &raw : sources) {
        const QString path = stripPrefix(raw);
        if (path.isEmpty() || !targetSize.isValid() || targetSize.isEmpty()) {
            continue;
        }
        const QString key = textureKey(path, targetSize);
        if (m_decodedScaled.contains(key) || m_predecoding.contains(key)) {
            continue;
        }
        m_predecoding.append(key);
        QFuture<void> f = QtConcurrent::run(QThreadPool::globalInstance(), [this, path, key, targetSize]() {
            QImage img = loadSourceImage(path, targetSize);
            if (!img.isNull()) {
                const QSize scaled = img.size().scaled(targetSize, Qt::KeepAspectRatio);
                if (scaled.width() < img.width() || scaled.height() < img.height()) {
                    img = img.scaled(scaled, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
            }
            QMetaObject::invokeMethod(
                this,
                [this, path, key, img]() {
                    m_predecoding.removeAll(key);
                    if (!img.isNull()) {
                        m_decodedScaled.insert(key, img);
                        Q_EMIT predecoded(path);
                    }
                },
                Qt::QueuedConnection);
        });
        m_predecodeSync.addFuture(f);
    }
}

ImageProvider::~ImageProvider()
{
    m_predecodeSync.waitForFinished();
}

}

/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwingltexture.h>

#include <QColor>
#include <QFont>
#include <QFutureSynchronizer>
#include <QThreadPool>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QVector>

#include <memory>

namespace KWin
{

class GLPaintRenderer;

class ImageProvider : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Constructs the image provider.
     * @param parent Parent object.
     */
    explicit ImageProvider(QObject *parent = nullptr);

    /**
     * @brief Destroys the image provider.
     */
    ~ImageProvider() override;

    struct TextureInfo
    {
        GLTexture *texture = nullptr;
        QSize imageSize;
        QSize textureSize;
        QString source;
    };

    /**
     * @brief Returns a texture for a source, decoding synchronously if necessary.
     * @param source Image source path or URI.
     * @param targetSize Target downsampled size.
     * @return The texture info; texture is nullptr when not yet decoded.
     */
    TextureInfo textureFor(const QString &source, const QSize &targetSize);

    /**
     * @brief Returns a ready texture, decoding it non-blockingly if needed.
     * @param source Image source path or URI.
     * @param targetSize Target downsampled size.
     * @param smooth Whether to request smooth filtering.
     * @return The texture info; imageSize is empty until decoded.
     */
    TextureInfo textureForReady(const QString &source, const QSize &targetSize, bool smooth = true);

    /**
     * @brief Builds the cache key for a source at a target size.
     * @param source Image source path or URI.
     * @param size Target size.
     * @return The cache key.
     */
    static QString textureKeyFor(const QString &source, const QSize &size);

    /**
     * @brief Returns a texture rendering the given text.
     * @param text Text to render.
     * @param font Font to use.
     * @param color Text color.
     * @return The text texture info.
     */
    TextureInfo textTexture(const QString &text, const QFont &font, const QColor &color);

    /**
     * @brief Drops every cached GL texture, keeping the decoded images for re-upload.
     */
    void invalidate();

    /**
     * @brief Evicts unused, unpinned resources.
     */
    void trimUnused();

    /**
     * @brief Marks a texture as in use, reserving its slot without loading it.
     * @param source Image source path or URI.
     * @param targetSize Target downsampled size.
     */
    void acquireTexture(const QString &source, const QSize &targetSize);

    /**
     * @brief Releases a use of a texture, allowing later eviction.
     * @param source Image source path or URI.
     * @param targetSize Target downsampled size.
     */
    void releaseTexture(const QString &source, const QSize &targetSize);

    /**
     * @brief Pins a texture so it is never evicted while pinned.
     * @param source Image source path or URI.
     * @param targetSize Target downsampled size.
     */
    void pin(const QString &source, const QSize &targetSize);

    /**
     * @brief Unpins a previously pinned texture.
     * @param source Image source path or URI.
     * @param targetSize Target downsampled size.
     */
    void unpin(const QString &source, const QSize &targetSize);

    /**
     * @brief Predecodes and downscales a batch of sources off the UI thread.
     * @param sources Image sources to decode.
     * @param targetSize Target size the decoded image is scaled to before first use.
     *
     * Decoding is CPU bound and the compositor needs the core, so the pool behind this
     * runs one image at a time, and nothing here is ever waited for: the paint side asks
     * with textureForReady() and paints a plain colour until the entry lands.
     */
    void predecode(const QStringList &sources, const QSize &targetSize);

    /**
     * @brief Returns the number of predecoded images.
     * @return Predecoded image count.
     */
    int decodedCount() const
    {
        return m_decodedScaled.size();
    }

    /**
     * @brief Returns the number of cached resources.
     * @return Cached resource count.
     */
    int cachedCount() const
    {
        return m_resources.size();
    }

Q_SIGNALS:
    /**
     * @brief Emitted when a predecode of a source completes.
     * @param source Image source that was decoded.
     */
    void predecoded(const QString &source);

private:
    TextureInfo textureForImpl(const QString &source, const QSize &targetSize, bool allowSyncLoad, bool smooth);
    struct Resource
    {
        QString source;
        QImage image;
        QSize imageSize;
        std::shared_ptr<GLTexture> texture;
        QSize textureSize;
        int useCount = 0;
        bool dirty = true;
        bool pinned = false;
    };

    TextureInfo publishImage(Resource &res);
    void ensureTexture(Resource *res);

    QHash<QString, Resource> m_resources;
    GLPaintRenderer *m_renderer = nullptr;
    QHash<QString, QImage> m_decodedScaled;
    QStringList m_predecoding;
    QFutureSynchronizer<void> m_predecodeSync;
    QThreadPool m_predecodePool;
};

}

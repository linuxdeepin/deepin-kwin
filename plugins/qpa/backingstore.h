// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_QPA_BACKINGSTORE_H
#define KWIN_QPA_BACKINGSTORE_H

#include <qpa/qplatformbackingstore.h>

namespace KWayland
{
namespace Client
{
class Buffer;
class ShmPool;
}
}

namespace KWin
{
namespace QPA
{

class BackingStore : public QPlatformBackingStore
{
public:
    explicit BackingStore(QWindow *w, KWayland::Client::ShmPool *shm);
    virtual ~BackingStore();

    QPaintDevice *paintDevice() override;
    void flush(QWindow *window, const QRegion &region, const QPoint &offset) override;
    void resize(const QSize &size, const QRegion &staticContents) override;
    void beginPaint(const QRegion &) override;

private:
    int scale() const;
    KWayland::Client::ShmPool *m_shm;
    QWeakPointer<KWayland::Client::Buffer> m_buffer;
    QImage m_backBuffer;
    QSize m_size;
};

}
}

#endif

// Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "decorationrenderer.h"
#include "decoratedclient.h"
#include "deleted.h"
#include "abstract_client.h"
#include "screens.h"

#include <KDecoration2/Decoration>
#include <KDecoration2/DecoratedClient>

#include <QDebug>
#include <QPainter>

namespace KWin
{
namespace Decoration
{

Renderer::Renderer(DecoratedClientImpl *client)
    : QObject(client)
    , m_client(client)
    , m_imageSizesDirty(true)
{
    auto markImageSizesDirty = [this]{
        m_imageSizesDirty = true;
    };
    connect(client->client(), &AbstractClient::screenScaleChanged, this, markImageSizesDirty);
    connect(client->decoration(), &KDecoration2::Decoration::bordersChanged, this, markImageSizesDirty);
    connect(client->decoratedClient(), &KDecoration2::DecoratedClient::widthChanged, this, markImageSizesDirty);
    connect(client->decoratedClient(), &KDecoration2::DecoratedClient::heightChanged, this, markImageSizesDirty);
}

Renderer::~Renderer() = default;

void Renderer::schedule(const QRect &rect)
{
    m_scheduled = m_scheduled.united(rect);
    emit renderScheduled(rect);
}

QRegion Renderer::getScheduled()
{
    QRegion region = m_scheduled;
    m_scheduled = QRegion();
    return region;
}

QImage Renderer::renderToImage(const QRect &geo)
{
    Q_ASSERT(m_client);
    auto dpr = client()->client()->screenScale();
    QImage image(geo.width() * dpr, geo.height() * dpr, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    p.setWindow(QRect(geo.topLeft(), geo.size() * dpr));
    p.setClipRect(geo);
    client()->decoration()->paint(&p, geo);
    return image;
}

void Renderer::reparent(Deleted *deleted)
{
    setParent(deleted);
    m_client = nullptr;
}

}
}

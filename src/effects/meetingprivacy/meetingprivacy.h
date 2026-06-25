// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <kwineffects.h>
#include <kwinglutils.h>

#include <QHash>
#include <QRegion>
#include <QSize>
#include <memory>

namespace KWin
{

class GLTexture;

class MeetingPrivacy : public Effect
{
    Q_OBJECT

public:
    MeetingPrivacy();
    ~MeetingPrivacy() override;

    static bool supported();
    static bool enabledByDefault();

    int requestedEffectChainPosition() const override
    {
        return 95;
    }

    void drawWindow(EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data) override;

    bool isActive() const override;

private Q_SLOTS:
    void slotWindowClosed(EffectWindow *w);

private:
    bool shouldProtect(EffectWindow *w) const;
    std::shared_ptr<GLTexture> getMaskTexture(const QSize &size);

    QHash<QSize, std::shared_ptr<GLTexture>> m_maskTextureCache;
};

}

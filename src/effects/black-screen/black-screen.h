// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef BLACKSCREENEFFECT_H
#define BLACKSCREENEFFECT_H

#include <deepin_kwineffects.h>
#include <deepin_kwinglplatform.h>
#include <deepin_kwinglutils.h>
#include <QDBusContext>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusUnixFileDescriptor>

using namespace KWin;

class BlackScreenEffect : public KWin::Effect, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.BlackScreen")
public:
    static bool supported();

    BlackScreenEffect(QObject *parent = nullptr, const QVariantList &args = QVariantList());
    ~BlackScreenEffect() override;
    void drawWindow(KWin::EffectWindow* w, int mask, const QRegion &region, KWin::WindowPaintData& data) override;
    virtual bool isActive() const override;
    int requestedEffectChainPosition() const override {
        return 1;
    }
public Q_SLOTS:
    Q_SCRIPTABLE void setActive(bool active);
    Q_SCRIPTABLE bool getActive();
private:
    bool m_activated {false};
};

#endif // BLACKSCREENEFFECT_H

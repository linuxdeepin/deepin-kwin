/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwineffects.h>
#include <kwinglutils.h>

#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QTimer>

namespace KWin
{

class BlackScreenEffect : public KWin::Effect, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.BlackScreen")
public:
    static bool supported();

    BlackScreenEffect(QObject *parent = nullptr, const QVariantList &args = QVariantList());
    ~BlackScreenEffect() override;

    void drawWindow(EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data) override;
    bool isActive() const override;
    int requestedEffectChainPosition() const override
    {
        return 1;
    }
public Q_SLOTS:
    Q_SCRIPTABLE void setActive(bool active);
    Q_SCRIPTABLE bool getActive() const;
    Q_SCRIPTABLE void start(quint32 ms);
private Q_SLOTS:
    void slotTimerTimeout();

private:
    QString dbusCaller() const;
    bool m_activated{false};
    QTimer m_timer;
};

} // namespace KWin

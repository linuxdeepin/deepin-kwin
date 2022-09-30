// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_QPA_NATIVEINTERFACE_H
#define KWIN_QPA_NATIVEINTERFACE_H

#include <qpa/qplatformnativeinterface.h>

namespace KWin
{
namespace QPA
{

class Integration;

class NativeInterface : public QPlatformNativeInterface
{
public:
    explicit NativeInterface(Integration *integration);
    void *nativeResourceForIntegration(const QByteArray &resource) override;
    void *nativeResourceForWindow(const QByteArray &resourceString, QWindow *window) override;
    QFunctionPointer platformFunction(const QByteArray &function) const override;

private:
    Integration *m_integration;
};

}
}

#endif

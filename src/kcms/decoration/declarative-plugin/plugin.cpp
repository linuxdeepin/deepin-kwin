/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "plugin.h"
#include "buttonsmodel.h"
#include "previewbridge.h"
#include "previewbutton.h"
#include "previewclient.h"
#include "previewitem.h"
#include "previewsettings.h"

#include <KDecoration3/Decoration>
#include <KDecoration3/DecorationShadow>

namespace KDecoration3
{
namespace Preview
{

void Plugin::registerTypes(const char *uri)
{
    Q_ASSERT(QLatin1String(uri) == QLatin1String("org.kde.kwin.private.kdecoration"));
    qmlRegisterType<KDecoration3::Preview::BridgeItem>(uri, 1, 0, "Bridge");
    qmlRegisterType<KDecoration3::Preview::Settings>(uri, 1, 0, "Settings");
    qmlRegisterType<KDecoration3::Preview::PreviewItem>(uri, 1, 0, "Decoration");
    qmlRegisterType<KDecoration3::Preview::PreviewButtonItem>(uri, 1, 0, "Button");
    qmlRegisterType<KDecoration3::Preview::ButtonsModel>(uri, 1, 0, "ButtonsModel");
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    qmlRegisterType<KDecoration3::Preview::PreviewClient>();
    qmlRegisterType<KDecoration3::Decoration>();
    qmlRegisterType<KDecoration3::DecorationShadow>();
    qmlRegisterType<KDecoration3::Preview::PreviewBridge>();
#else
    qmlRegisterAnonymousType<KDecoration3::Preview::PreviewClient>(uri, 1);
    qmlRegisterAnonymousType<KDecoration3::Decoration>(uri, 1);
    qmlRegisterAnonymousType<KDecoration3::DecorationShadow>(uri, 1);
    qmlRegisterAnonymousType<KDecoration3::Preview::PreviewBridge>(uri, 1);
#endif

}

}
}

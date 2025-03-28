/*
    SPDX-FileCopyrightText: 2017 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "windowswitcher.h"

#include <KLocalizedString>

void SwitcherPackage::initPackage(KPackage::Package *package)
{
    package->setDefaultPackageRoot(QStringLiteral("kwin/tabbox/"));
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    package->addDirectoryDefinition("config", QStringLiteral("config"), i18n("Configuration Definitions"));
#else
    package->addDirectoryDefinition("config", QStringLiteral("config"));
#endif
    QStringList mimetypes;
    mimetypes << QStringLiteral("text/xml");
    package->setMimeTypes("config", mimetypes);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    package->addDirectoryDefinition("ui", QStringLiteral("ui"), i18n("User Interface"));
    package->addDirectoryDefinition("code", QStringLiteral("code"), i18n("Executable windowswitcher"));
    package->addFileDefinition("mainscript", QStringLiteral("ui/main.qml"), i18n("Main Script File"));
#else
    package->addDirectoryDefinition("ui", QStringLiteral("ui"));
    package->addDirectoryDefinition("code", QStringLiteral("code"));
    package->addFileDefinition("mainscript", QStringLiteral("ui/main.qml"));
#endif
    package->setRequired("mainscript", true);

    mimetypes.clear();
    mimetypes << QStringLiteral("text/plain");
    package->setMimeTypes("windowswitcher", mimetypes);
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void SwitcherPackage::pathChanged(KPackage::Package *package)
{
    if (package->path().isEmpty()) {
        return;
    }
    KPluginMetaData md(package->metadata().metaDataFileName());
    QString mainScript = md.value("X-Plasma-MainScript");
    if (!mainScript.isEmpty()) {
        package->addFileDefinition("mainscript", mainScript, i18n("Main Script File"));
    }
}
#endif

K_PLUGIN_CLASS_WITH_JSON(SwitcherPackage, "kwin-packagestructure-windowswitcher.json")

#include "windowswitcher.moc"

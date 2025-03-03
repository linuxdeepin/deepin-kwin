/*
    SPDX-FileCopyrightText: 2017 Demitrius Belai <demitriusbelai@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "decoration.h"

#include <KLocalizedString>

void DecorationPackage::initPackage(KPackage::Package *package)
{
    package->setDefaultPackageRoot(QStringLiteral("kwin/decorations/"));

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
    package->addDirectoryDefinition("code", QStringLiteral("code"), i18n("Executable Scripts"));
    package->addFileDefinition("mainscript", QStringLiteral("code/main.qml"), i18n("Main Script File"));
#else
    package->addDirectoryDefinition("ui", QStringLiteral("ui"));
    package->addDirectoryDefinition("code", QStringLiteral("code"));
    package->addFileDefinition("mainscript", QStringLiteral("code/main.qml"));
#endif
    package->setRequired("mainscript", true);

    mimetypes.clear();
    mimetypes << QStringLiteral("text/plain");
    package->setMimeTypes("decoration", mimetypes);
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void DecorationPackage::pathChanged(KPackage::Package *package)
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

K_PLUGIN_CLASS_WITH_JSON(DecorationPackage, "kwin-packagestructure-decoration.json")

#include "decoration.moc"

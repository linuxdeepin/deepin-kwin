/*
    SPDX-FileCopyrightText: 2017 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "scripts.h"

#include <KLocalizedString>

void ScriptsPackage::initPackage(KPackage::Package *package)
{
    package->setDefaultPackageRoot(QStringLiteral("kwin/scripts/"));
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
    package->addFileDefinition("mainscript", QStringLiteral("code/main.js"), i18n("Main Script File"));
#else
    package->addDirectoryDefinition("ui", QStringLiteral("ui"));
    package->addDirectoryDefinition("code", QStringLiteral("code"));
    package->addFileDefinition("mainscript", QStringLiteral("code/main.js"));
#endif
    package->setRequired("mainscript", true);

    mimetypes.clear();
    mimetypes << QStringLiteral("text/plain");
    package->setMimeTypes("scripts", mimetypes);
}

void ScriptsPackage::pathChanged(KPackage::Package *package)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (package->path().isEmpty()) {
        return;
    }

    KPluginMetaData md(package->metadata().metaDataFileName());
    QString mainScript = md.value("X-Plasma-MainScript");

    if (!mainScript.isEmpty()) {
        package->addFileDefinition("mainscript", mainScript, i18n("Main Script File"));
    }
#else
    if (!package->metadata().isValid()) {
        return;
    }

    const QString api = package->metadata().value(QStringLiteral("X-Plasma-API"));
    if (api == QStringLiteral("javascript")) {
        package->addFileDefinition("mainscript", QStringLiteral("code/main.js"));
        package->setRequired("mainscript", true);
    } else if (api == QStringLiteral("declarativescript")) {
        package->addFileDefinition("mainscript", QStringLiteral("ui/main.qml"));
        package->setRequired("mainscript", true);
    }
#endif
}

K_PLUGIN_CLASS_WITH_JSON(ScriptsPackage, "kwin-packagestructure-scripts.json")

#include "scripts.moc"

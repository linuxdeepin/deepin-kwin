/*
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "effect.h"

#include <KLocalizedString>

EffectPackageStructure::EffectPackageStructure(QObject *parent, const QVariantList &args)
    : KPackage::PackageStructure(parent, args)
{
}

void EffectPackageStructure::initPackage(KPackage::Package *package)
{
    package->setDefaultPackageRoot(QStringLiteral("kwin/effects/"));
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    package->addDirectoryDefinition("code", QStringLiteral("code"), i18n("Executable Scripts"));
    package->setMimeTypes("code", {QStringLiteral("text/plain")});

    package->addFileDefinition("mainscript", QStringLiteral("code/main.js"), i18n("Main Script File"));
    package->setRequired("mainscript", true);

    package->addFileDefinition("config", QStringLiteral("config/main.xml"), i18n("Configuration Definition File"));
    package->setMimeTypes("config", {QStringLiteral("text/xml")});

    package->addFileDefinition("configui", QStringLiteral("ui/config.ui"), i18n("KCM User Interface File"));
    package->setMimeTypes("configui", {QStringLiteral("text/xml")});
#else
    package->addDirectoryDefinition("code", QStringLiteral("code"));
    package->setMimeTypes("code", {QStringLiteral("text/plain")});

    package->addFileDefinition("mainscript", QStringLiteral("code/main.js"));
    package->setRequired("mainscript", true);

    package->addFileDefinition("config", QStringLiteral("config/main.xml"));
    package->setMimeTypes("config", {QStringLiteral("text/xml")});

    package->addFileDefinition("configui", QStringLiteral("ui/config.ui"));
    package->setMimeTypes("configui", {QStringLiteral("text/xml")});
#endif
}

void EffectPackageStructure::pathChanged(KPackage::Package *package)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (package->path().isEmpty()) {
        return;
    }

    const KPluginMetaData md(package->metadata().metaDataFileName());
    const QString mainScript = md.value("X-Plasma-MainScript");
    if (mainScript.isEmpty()) {
        return;
    }

    package->addFileDefinition("mainscript", mainScript, i18n("Main Script File"));
#else
    if (!package->metadata().isValid()) {
        return;
    }

    const QString api = package->metadata().value(QStringLiteral("X-Plasma-API"));
    if (api == QLatin1StringView("javascript")) {
        package->addFileDefinition("mainscript", QStringLiteral("code/main.js"));
        package->setRequired("mainscript", true);
    } else if (api == QLatin1StringView("declarativescript")) {
        package->addFileDefinition("mainscript", QStringLiteral("ui/main.qml"));
        package->setRequired("mainscript", true);
    }
#endif
}

K_PLUGIN_CLASS_WITH_JSON(EffectPackageStructure, "kwin-packagestructure-effect.json")

#include "effect.moc"

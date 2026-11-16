/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>

namespace KWin
{

/**
 * @brief Builds a path to a compiled-in plugin resource.
 * @param sub Resource subdirectory.
 * @param name Resource file name.
 * @return The resource path.
 */
inline QString resourcePath(const QString &sub, const QString &name)
{
    return QStringLiteral(":/effects/multitaskviewex/") + sub + QLatin1Char('/') + name;
}

/**
 * @brief Builds a path to a button asset.
 * @param name Button file name.
 * @return The resource path.
 */
inline QString assetPath(const QString &name)
{
    return resourcePath(QStringLiteral("buttons"), name);
}

/**
 * @brief Builds a path to a shader resource.
 * @param name Shader file name.
 * @return The resource path.
 */
inline QString shaderPath(const QString &name)
{
    return resourcePath(QStringLiteral("shaders"), name);
}

/**
 * @brief Returns the path of the "top" toolbar icon.
 * @param active Whether the window is kept above.
 * @return The asset path.
 */
inline QString topIconPath(bool active)
{
    return assetPath(active ? QStringLiteral("multiview_top_active.svg")
                            : QStringLiteral("multiview_top.svg"));
}

/**
 * @brief Returns the path of the window delete icon.
 * @return The asset path.
 */
inline QString deleteIconPath()
{
    return assetPath(QStringLiteral("multiview_delete.svg"));
}

/**
 * @brief Returns the path of the add button icon.
 * @return The asset path.
 */
inline QString addIconPath()
{
    return assetPath(QStringLiteral("add-light.svg"));
}

/**
 * @brief Returns the path of the workspace delete icon.
 * @return The asset path.
 */
inline QString workspaceDeleteIconPath()
{
    return assetPath(QStringLiteral("workspace_delete.svg"));
}

}

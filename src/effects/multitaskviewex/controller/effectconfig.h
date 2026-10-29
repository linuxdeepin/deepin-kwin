/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "model/motionoptions.h"

#include <QDBusObjectPath>
#include <QObject>
#include <QString>

namespace KWin
{

/**
 * @brief Reads and caches the effect settings exposed through DConfig.
 *
 * The configuration is fetched once when the effect is constructed and refetched
 * whenever KWin reconfigures the effect or the DConfig value changes.
 */
class EffectConfig : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the configuration reader.
     * @param parent Parent object.
     */
    explicit EffectConfig(QObject *parent = nullptr);

    /**
     * @brief Acquires the DConfig manager and subscribes to its change notifications.
     */
    void init();

    /**
     * @brief Re-reads every setting and emits changed().
     */
    void reload();

    /**
     * @brief Tests whether windows should be blurred while the view is active.
     * @return True when the blur role is applied.
     */
    bool useBlurEffect() const
    {
        return m_useBlurEffect;
    }

    /**
     * @brief Returns the maximum number of workspaces the view allows.
     * @return The workspace limit.
     */
    int maxWorkspaces() const
    {
        return m_maxWorkspaces;
    }

    /**
     * @brief Returns the animation timing options.
     * @return The current motion options.
     */
    const MotionOptions &motion() const
    {
        return m_motion;
    }

Q_SIGNALS:
    /**
     * @brief Emitted when the cached settings were replaced.
     */
    void changed();

private Q_SLOTS:
    /**
     * @brief Reloads the settings after a DConfig value changed.
     * @param key Key that changed.
     */
    void onDConfigChanged(const QString &key);

private:
    template<typename T>
    T read(const QString &key, const T &defaultValue);

    QDBusObjectPath m_path;
    bool m_useBlurEffect = false;
    int m_maxWorkspaces = kDefaultMaxWorkspaces;
    MotionOptions m_motion;
};

}

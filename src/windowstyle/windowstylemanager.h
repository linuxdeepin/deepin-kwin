/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 zhang yu <zhangyud@uniontech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef WINDOWSTYLEMANAGER_H
#define WINDOWSTYLEMANAGER_H

#include <QObject>
#include <memory>

namespace KWin
{
class Window;
class Unmanaged;
class ConfigReader;

class WindowStyleManager : public QObject
{
    Q_OBJECT
public:
    explicit WindowStyleManager();
    ~WindowStyleManager();

public:
    float getOsRadius();
    float getOsScale();

Q_SIGNALS:
    void sigRadiusChanged(float &);
    void sigThemeChanged(bool &);

public Q_SLOTS:
    void onRadiusChange(QVariant);
    void onThemeChange(QVariant);
    void onWindowAdded(Window*);
    void onWindowMaxiChanged(Window *, bool, bool);
    void onWindowActiveChanged();
    void onGeometryShapeChanged(Window *, QRectF);
    void onCompositingChanged(bool);

private:
    std::unique_ptr<ConfigReader> m_radiusConfig;
    std::unique_ptr<ConfigReader> m_themeConfig;
    float        m_osRadius = -1.0;
    float        m_scale = 1.0;
};
}

#endif
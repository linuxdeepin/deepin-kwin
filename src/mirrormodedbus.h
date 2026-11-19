/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>
#include <QStringList>

namespace KWin
{

class Window;
class Workspace;

// The mirror mode window list, published at /MirrorMode.
class MirrorModeDBus : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.MirrorMode")
    Q_PROPERTY(QStringList primaryWindows READ primaryWindows NOTIFY windowsChanged)
    Q_PROPERTY(QStringList mirrorWindows READ mirrorWindows NOTIFY windowsChanged)
public:
    explicit MirrorModeDBus(Workspace *workspace);

    QStringList primaryWindows() const;
    QStringList mirrorWindows() const;

public Q_SLOTS:
    bool approveWindow(qulonglong winId);
    bool removeWindow(qulonglong winId);

Q_SIGNALS:
    void windowsChanged();

private:
    Window *findWindow(qulonglong winId) const;

    Workspace *const m_workspace;
};

} // namespace KWin

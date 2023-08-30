/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 zhang yu <zhangyud@uniontech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef SPLITMANAGER_H
#define SPLITMANAGER_H

#include "../utils/common.h"
#include "splitbar.h"

namespace KWin {

class Window;
// class Workspace;

class SplitGroup
{
public:
    SplitGroup(int);
    ~SplitGroup(){};

    int desktop() {return m_desktop;};
    void setDesktop(int);
    void storeSplitWindow(Window *);
    void deleteSplitWindow(Window *);
    void getSplitWindow(QVector<Window *> &);

private:
    int m_desktop;
    QVector<Window *> m_splitGroup;
};

class SplitManage : public QObject
{
    Q_OBJECT

public:
    explicit SplitManage(){};
    ~SplitManage(){};

    void add(Window *window);
    void remove(Window *window);
    // void unmanagedAdd(Window *window);
    void getSplitWindows(QHash<QString, QVector<Window *>> &);
    void getSplitBarWindow(QHash<QString, Window *> &);

    void inhibit();
    void uninhibit();

Q_SIGNALS:
    // void signalWindowGeometry(QString, Window *);
    void signalSplitWindow(QString &, Window *w);

public Q_SLOTS:
    void updateSplitWindowGeometry(QString, QPointF, bool);

private:
    struct WindowData
    {
        // QUuid outputUuid;
        QRectF geometry;
        // MaximizeMode maximize;
        QuickTileMode quickTile;
        // QRectF geometryRestore;
        // bool fullscreen;
        // QRectF fullscreenGeometryRestore;
        // uint32_t interactiveMoveResizeCount;
        QString screenName;
        int desktop;
    };

private:
    void handleQuickTile();
    bool isSplitWindow(Window *window);
    void createSplitBar(QString &);
    SplitGroup *createGroup(int &, QString &);
    SplitGroup *getGroup(int &, QString &);
    WindowData dataForWindow(Window *window) const;

private:
    int m_inhibitCount = 0;
    QHash<QString, QSet<SplitGroup *>> m_splitGroupManage;
    QHash<QString, SplitBar *> m_splitBarManage;
    QHash<QString, Window *> m_splitBarWindows;
    // QHash<QString, QHash<Window *, WindowData>> m_data;
    // QVector<Window *> m_savedWindow;
    QHash<Window *, WindowData> m_data;
};
}

#endif

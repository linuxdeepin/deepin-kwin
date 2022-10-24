// Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_MAIN_WAYLAND_H
#define KWIN_MAIN_WAYLAND_H
#include "main.h"
#include <QProcessEnvironment>

class QProcess;

namespace KWin
{

namespace Xwl
{
class Xwayland;
}

class ApplicationWayland : public ApplicationWaylandAbstract
{
    Q_OBJECT
public:
    ApplicationWayland(int &argc, char **argv);
    virtual ~ApplicationWayland();

    void setStartXwayland(bool start) {
        m_startXWayland = start;
    }
    void setApplicationsToStart(const QStringList &applications) {
        m_applicationsToStart = applications;
    }
    void setInputMethodServerToStart(const QString &inputMethodServer) {
        m_inputMethodServerToStart = inputMethodServer;
    }
    void setProcessStartupEnvironment(const QProcessEnvironment &environment) override {
        m_environment = environment;
    }
    void setSessionArgument(const QString &session) {
        m_sessionArgument = session;
    }

    void setWithoutScreen(bool withoutScreen) {
        m_runWithoutScreen = withoutScreen;
    }

    void setDisableMultiScreens(bool disabled) {
        m_disableMultiScreens = disabled;
    }

    QProcessEnvironment processStartupEnvironment() const override {
        return m_environment;
    }

protected:
    void performStartup() override;

private:
    void createBackend();
    void continueStartupWithScreens();
    void continueStartupWithoutScreens();
    void continueStartupWithScene();
    void continueStartupWithXwayland();
    void startSession() override;

    bool m_startXWayland = false;
    QStringList m_applicationsToStart;
    QString m_inputMethodServerToStart;
    QProcessEnvironment m_environment;
    QString m_sessionArgument;

    Xwl::Xwayland *m_xwayland = nullptr;
    bool m_runWithoutScreen = false;
    bool m_disableMultiScreens = false;
};

}

#endif

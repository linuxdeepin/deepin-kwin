# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(multitaskviewex_SOURCES
    main.cpp
    controller/multitaskviewex.cpp
    controller/effectconfig.cpp
    controller/session.cpp
    controller/bar.cpp
    controller/keynav.cpp
    controller/wallpaper.cpp
    controller/wallpapermanager.cpp
    controller/workspacemanager.cpp
    controller/windownavigator.cpp
    controller/windowroles.cpp
    model/scenemodel.cpp
    view/glscene.cpp
    view/barlayer.cpp
    view/outgoinglayer.cpp
    view/glpaintrenderer.cpp
    view/glcontrol.cpp
    view/backgroundcontrol.cpp
    view/workspacebarcontrol.cpp
    view/windowcellcontrol.cpp
    view/windowoverlaycontrol.cpp
    view/dragghostcontrol.cpp
    view/imageprovider.cpp
    model/windowgrid.cpp
    animation/animation.cpp
    data/workspacemodel.cpp
    data/wallpaperprovider.cpp
    multitaskviewex.qrc
)

kwin4_add_effect_module(multitaskviewex ${multitaskviewex_SOURCES})

target_link_libraries(multitaskviewex PRIVATE
    kwineffects
    KF${KF_MAJOR_VERSION}::GlobalAccel
    KF${KF_MAJOR_VERSION}::ConfigGui
    KF${KF_MAJOR_VERSION}::I18n
    Qt${QT_MAJOR_VERSION}::DBus
    Qt${QT_MAJOR_VERSION}::Quick
    Qt${QT_MAJOR_VERSION}::Qml
    Qt${QT_MAJOR_VERSION}::Svg
    Qt${QT_MAJOR_VERSION}::Concurrent
)

target_include_directories(multitaskviewex PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

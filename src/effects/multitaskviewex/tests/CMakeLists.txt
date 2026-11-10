# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: GPL-2.0-or-later

find_package(Qt${QT_MAJOR_VERSION} REQUIRED COMPONENTS Core Gui Test)
find_package(PkgConfig REQUIRED)
pkg_check_modules(EPOXY REQUIRED IMPORTED_TARGET epoxy)

set(MTO_SRC ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_executable(tst_animation
    tst_animation.cpp
    ${MTO_SRC}/animation/animation.cpp
)
target_include_directories(tst_animation PRIVATE ${MTO_SRC})
target_link_libraries(tst_animation PRIVATE Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Test)
set_target_properties(tst_animation PROPERTIES AUTOMOC ON)
add_test(NAME multitaskviewex_tst_animation COMMAND tst_animation)

add_executable(tst_windowgrid
    tst_windowgrid.cpp
    ${MTO_SRC}/model/windowgrid.cpp
    ${MTO_SRC}/animation/animation.cpp
)
target_include_directories(tst_windowgrid PRIVATE ${MTO_SRC})
target_link_libraries(tst_windowgrid PRIVATE Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Gui Qt${QT_MAJOR_VERSION}::Test)
set_target_properties(tst_windowgrid PROPERTIES AUTOMOC ON)
add_test(NAME multitaskviewex_tst_windowgrid COMMAND tst_windowgrid -platform offscreen)

add_executable(tst_bardragmodel
    tst_bardragmodel.cpp
)
target_include_directories(tst_bardragmodel PRIVATE ${MTO_SRC})
target_link_libraries(tst_bardragmodel PRIVATE Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Test)
set_target_properties(tst_bardragmodel PROPERTIES AUTOMOC ON)
add_test(NAME multitaskviewex_tst_bardragmodel COMMAND tst_bardragmodel)

add_executable(tst_baritem
    tst_baritem.cpp
    ${MTO_SRC}/animation/animation.cpp
    ${MTO_SRC}/view/barlayer.cpp
    ${MTO_SRC}/view/outgoinglayer.cpp
)
target_include_directories(tst_baritem PRIVATE ${MTO_SRC})
target_link_libraries(tst_baritem PRIVATE Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Test)
set_target_properties(tst_baritem PROPERTIES AUTOMOC ON)
add_test(NAME multitaskviewex_tst_baritem COMMAND tst_baritem)

add_executable(tst_wallpaperdata
    tst_wallpaperdata.cpp
)
target_include_directories(tst_wallpaperdata PRIVATE ${MTO_SRC})
target_link_libraries(tst_wallpaperdata PRIVATE Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Test)
set_target_properties(tst_wallpaperdata PROPERTIES AUTOMOC ON)
add_test(NAME multitaskviewex_tst_wallpaperdata COMMAND tst_wallpaperdata)

add_executable(tst_session
    tst_session.cpp
    ${MTO_SRC}/animation/animation.cpp
)
target_include_directories(tst_session PRIVATE ${MTO_SRC})
target_link_libraries(tst_session PRIVATE Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Gui Qt${QT_MAJOR_VERSION}::Test)
set_target_properties(tst_session PROPERTIES AUTOMOC ON)
add_test(NAME multitaskviewex_tst_session COMMAND tst_session -platform offscreen)

add_executable(tst_multiscreen
    tst_multiscreen.cpp
    ${MTO_SRC}/model/scenemodel.cpp
    ${MTO_SRC}/view/glscene.cpp
    ${MTO_SRC}/view/barlayer.cpp
    ${MTO_SRC}/view/outgoinglayer.cpp
    ${MTO_SRC}/view/glcontrol.cpp
    ${MTO_SRC}/view/glpaintrenderer.cpp
    ${MTO_SRC}/view/backgroundcontrol.cpp
    ${MTO_SRC}/view/windowcellcontrol.cpp
    ${MTO_SRC}/view/windowoverlaycontrol.cpp
    ${MTO_SRC}/view/hintcontrol.cpp
    ${MTO_SRC}/view/workspacebarcontrol.cpp
    ${MTO_SRC}/view/dragghostcontrol.cpp
    ${MTO_SRC}/view/imageprovider.cpp
    ${MTO_SRC}/model/windowgrid.cpp
    ${MTO_SRC}/animation/animation.cpp
)
target_include_directories(tst_multiscreen PRIVATE ${MTO_SRC})
target_link_libraries(tst_multiscreen PRIVATE Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Gui Qt${QT_MAJOR_VERSION}::Svg Qt${QT_MAJOR_VERSION}::Concurrent Qt${QT_MAJOR_VERSION}::Test PkgConfig::EPOXY kwinglutils)
set_target_properties(tst_multiscreen PROPERTIES AUTOMOC ON)
add_test(NAME multitaskviewex_tst_multiscreen COMMAND tst_multiscreen -platform offscreen)

add_executable(tst_scenemodel
    tst_scenemodel.cpp
    ${MTO_SRC}/model/scenemodel.cpp
    ${MTO_SRC}/model/windowgrid.cpp
    ${MTO_SRC}/animation/animation.cpp
)
target_include_directories(tst_scenemodel PRIVATE ${MTO_SRC})
target_link_libraries(tst_scenemodel PRIVATE Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Test)
set_target_properties(tst_scenemodel PROPERTIES AUTOMOC ON)
add_test(NAME multitaskviewex_tst_scenemodel COMMAND tst_scenemodel)

add_executable(tst_textutils
    tst_textutils.cpp
)
target_include_directories(tst_textutils PRIVATE ${MTO_SRC})
target_link_libraries(tst_textutils PRIVATE Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Gui Qt${QT_MAJOR_VERSION}::Test)
set_target_properties(tst_textutils PROPERTIES AUTOMOC ON)
add_test(NAME multitaskviewex_tst_textutils COMMAND tst_textutils -platform offscreen)

add_executable(tst_scene
    tst_scene.cpp
    ${MTO_SRC}/model/scenemodel.cpp
    ${MTO_SRC}/view/glscene.cpp
    ${MTO_SRC}/view/barlayer.cpp
    ${MTO_SRC}/view/outgoinglayer.cpp
    ${MTO_SRC}/view/glcontrol.cpp
    ${MTO_SRC}/view/glpaintrenderer.cpp
    ${MTO_SRC}/view/backgroundcontrol.cpp
    ${MTO_SRC}/view/windowcellcontrol.cpp
    ${MTO_SRC}/view/windowoverlaycontrol.cpp
    ${MTO_SRC}/view/hintcontrol.cpp
    ${MTO_SRC}/view/workspacebarcontrol.cpp
    ${MTO_SRC}/view/dragghostcontrol.cpp
    ${MTO_SRC}/view/imageprovider.cpp
    ${MTO_SRC}/model/windowgrid.cpp
    ${MTO_SRC}/animation/animation.cpp
)
target_include_directories(tst_scene PRIVATE ${MTO_SRC})
target_link_libraries(tst_scene PRIVATE Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Gui Qt${QT_MAJOR_VERSION}::Svg Qt${QT_MAJOR_VERSION}::Concurrent Qt${QT_MAJOR_VERSION}::Test PkgConfig::EPOXY kwinglutils)
set_target_properties(tst_scene PROPERTIES AUTOMOC ON)
add_test(NAME multitaskviewex_tst_scene COMMAND tst_scene -platform offscreen)

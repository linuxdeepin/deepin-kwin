set(CMAKE_SAFETYTEST "${CMAKE_SAFETYTEST_ARG}")
if(CMAKE_SAFETYTEST STREQUAL "")
    set(CMAKE_SAFETYTEST "CMAKE_SAFETYTEST_ARG_OFF")
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D${CMAKE_SAFETYTEST}")
if(CMAKE_SAFETYTEST STREQUAL "CMAKE_SAFETYTEST_ARG_ON")
  #安全测试选项
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -fsanitize=address -O2")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fsanitize=address -O2")
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fprofile-arcs -ftest-coverage")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -fprofile-arcs -ftest-coverage")

add_definitions(-DKWIN_UNIT_TEST)
remove_definitions(-DQT_USE_QSTRINGBUILDER)

# 添加asan内存泄露基类
set(testprintasanbase_SRCS testprintasanbase.cpp)

########################################################
# Test ScreenPaintData
########################################################
set( testScreenPaintData_SRCS test_screen_paint_data.cpp)
add_executable(testScreenPaintData ${testScreenPaintData_SRCS} ${testprintasanbase_SRCS})
# sw and mips not support asan,we block it for the moment by MACRO
if ((${CMAKE_SYSTEM_PROCESSOR} STREQUAL "sw_64") OR (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "mips64"))
    add_definitions(-DNOTSUPPORTASAN)
    option(NOTSUPPORTASAN "ASAN no support" ON)
endif()
if (NOTSUPPORTASAN)
    set(ASANLIBS kwineffects Qt5::Test Qt5::Widgets KF5::WindowSystem)
else()
    set(ASANLIBS kwineffects Qt5::Test Qt5::Widgets KF5::WindowSystem libasan.a -lpthread -ldl -lrt)
endif()
target_link_libraries( testScreenPaintData ${ASANLIBS})
add_test(NAME kwin-testScreenPaintData COMMAND testScreenPaintData)
ecm_mark_as_test(testScreenPaintData)

add_subdirectory(libkwineffects)
add_subdirectory(libxrenderutils)
if(CMAKE_SAFETYTEST STREQUAL "CMAKE_SAFETYTEST_ARG_OFF")
add_subdirectory(integration)
endif()
add_subdirectory(libinput)
if (HAVE_DRM)
    add_subdirectory(drm)
endif()
add_subdirectory(tabbox)

########################################################
# Test WindowPaintData
########################################################
set( testWindowPaintData_SRCS test_window_paint_data.cpp )
add_executable(testWindowPaintData ${testWindowPaintData_SRCS} ${testprintasanbase_SRCS})
target_link_libraries( testWindowPaintData kwineffects Qt5::Widgets Qt5::Test )
add_test(NAME kwin-testWindowPaintData COMMAND testWindowPaintData)
ecm_mark_as_test(testWindowPaintData)

########################################################
# Test VirtualDesktopManager
########################################################
#[[set( testVirtualDesktops_SRCS
     test_virtual_desktops.cpp
     ../virtualdesktops.cpp
)
add_executable(testVirtualDesktops ${testVirtualDesktops_SRCS} ${testprintasanbase_SRCS})

target_link_libraries( testVirtualDesktops
                       Qt5::Test
                       Qt5::Widgets
                       KF5::I18n
                       KF5::GlobalAccel
                       KF5::ConfigCore
                       KF5::WindowSystem
                       KF5::WaylandServer
)
#add_test(NAME kwin-testVirtualDesktops COMMAND testVirtualDesktops)
#ecm_mark_as_test(testVirtualDesktops)
]]
########################################################
# Test ClientMachine
########################################################
set( testClientMachine_SRCS
     test_client_machine.cpp
     ../client_machine.cpp
)
add_executable( testClientMachine ${testClientMachine_SRCS} ${testprintasanbase_SRCS})
set_target_properties(testClientMachine PROPERTIES COMPILE_DEFINITIONS "NO_NONE_WINDOW")

target_link_libraries( testClientMachine
                       Qt5::Concurrent
                       Qt5::Test
                       Qt5::X11Extras
                       Qt5::Widgets
                       KF5::ConfigCore
                       KF5::WindowSystem
                       XCB::XCB
                       XCB::XFIXES
                       ${X11_X11_LIB} # to make jenkins happy
)
add_test(NAME kwin-testClientMachine COMMAND testClientMachine)
ecm_mark_as_test(testClientMachine)
########################################################
# Test XcbWrapper
########################################################
set( testXcbWrapper_SRCS
     test_xcb_wrapper.cpp
)
add_executable( testXcbWrapper ${testXcbWrapper_SRCS} ${testprintasanbase_SRCS})

target_link_libraries( testXcbWrapper
                       Qt5::Test
                       Qt5::X11Extras
                       Qt5::Widgets
                       KF5::ConfigCore
                       KF5::WindowSystem
                       XCB::XCB
)
add_test(NAME kwin-testXcbWrapper COMMAND testXcbWrapper)
ecm_mark_as_test(testXcbWrapper)

if (XCB_ICCCM_FOUND)
    add_executable( testXcbSizeHints test_xcb_size_hints.cpp ${testprintasanbase_SRCS})
    set_target_properties(testXcbSizeHints PROPERTIES COMPILE_DEFINITIONS "NO_NONE_WINDOW")
    target_link_libraries( testXcbSizeHints
                           Qt5::Test
                           Qt5::X11Extras
                           Qt5::Widgets
                           KF5::ConfigCore
                           KF5::WindowSystem
                           XCB::XCB
                           XCB::ICCCM
    )
    add_test(NAME kwin-testXcbSizeHints COMMAND testXcbSizeHints)
    ecm_mark_as_test(testXcbSizeHints)
endif()

########################################################
# Test XcbWindow
########################################################
set( testXcbWindow_SRCS
     test_xcb_window.cpp
)
add_executable( testXcbWindow ${testXcbWindow_SRCS} ${testprintasanbase_SRCS})

target_link_libraries( testXcbWindow
                       Qt5::Test
                       Qt5::X11Extras
                       Qt5::Widgets
                       KF5::ConfigCore
                       KF5::WindowSystem
                       XCB::XCB
)
add_test(NAME kwin-testXcbWindow COMMAND testXcbWindow)
ecm_mark_as_test(testXcbWindow)

########################################################
# Test BuiltInEffectLoader
########################################################
set( testBuiltInEffectLoader_SRCS
    test_builtin_effectloader.cpp
    mock_effectshandler.cpp
    ../effectloader.cpp
)
add_executable( testBuiltInEffectLoader ${testBuiltInEffectLoader_SRCS} ${testprintasanbase_SRCS})
set_target_properties(testBuiltInEffectLoader PROPERTIES COMPILE_DEFINITIONS "NO_NONE_WINDOW")

target_link_libraries(testBuiltInEffectLoader
    Qt5::Concurrent
    Qt5::Test
    Qt5::X11Extras
    KF5::Package
    kwineffects
    kwin4_effect_builtins
)

add_test(NAME kwin-testBuiltInEffectLoader COMMAND testBuiltInEffectLoader)
ecm_mark_as_test(testBuiltInEffectLoader)

########################################################
# Test ScriptedEffectLoader
########################################################
include_directories(${KWIN_SOURCE_DIR})
set( testScriptedEffectLoader_SRCS
    test_scripted_effectloader.cpp
    mock_abstract_client.cpp
    mock_effectshandler.cpp
    mock_screens.cpp
    mock_workspace.cpp
    ../effectloader.cpp
    ../scripting/scriptedeffect.cpp
    ../scripting/scriptingutils.cpp
    ../scripting/scripting_logging.cpp
    ../screens.cpp
    ../orientation_sensor.cpp
)
kconfig_add_kcfg_files(testScriptedEffectLoader_SRCS ../settings.kcfgc)
qt5_add_dbus_adaptor( testScriptedEffectLoader_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../org.kde.kwin.OrientationSensor.xml ${CMAKE_CURRENT_SOURCE_DIR}/../orientation_sensor.h KWin::OrientationSensor)
add_executable( testScriptedEffectLoader ${testScriptedEffectLoader_SRCS} ${testprintasanbase_SRCS})

target_link_libraries(testScriptedEffectLoader
    Qt5::Concurrent
    Qt5::Qml
    Qt5::Script
    Qt5::Sensors
    Qt5::Test
    Qt5::X11Extras
    KF5::ConfigGui
    KF5::GlobalAccel
    KF5::I18n
    KF5::Notifications
    KF5::Package
    kwineffects
    kwin4_effect_builtins
)

add_test(NAME kwin-testScriptedEffectLoader COMMAND testScriptedEffectLoader)
ecm_mark_as_test(testScriptedEffectLoader)

########################################################
# Test PluginEffectLoader
########################################################
#[[set( testPluginEffectLoader_SRCS
    test_plugin_effectloader.cpp
    mock_effectshandler.cpp
    ../effectloader.cpp
)
add_executable( testPluginEffectLoader ${testPluginEffectLoader_SRCS} ${testprintasanbase_SRCS})

target_link_libraries(testPluginEffectLoader
    Qt5::Concurrent
    Qt5::Test
    Qt5::X11Extras
    KF5::Package
    kwineffects
    kwin4_effect_builtins
)

#add_test(NAME kwin-testPluginEffectLoader COMMAND testPluginEffectLoader)
#ecm_mark_as_test(testPluginEffectLoader)
]]
########################################################
# FakeEffectPlugin
########################################################
add_library(fakeeffectplugin MODULE fakeeffectplugin.cpp)
set_target_properties(fakeeffectplugin PROPERTIES PREFIX "")
target_link_libraries(fakeeffectplugin kwineffects)

########################################################
# FakeEffectPlugin-Version
########################################################
add_library(effectversionplugin MODULE fakeeffectplugin_version.cpp)
set_target_properties(effectversionplugin PROPERTIES PREFIX "")
target_link_libraries(effectversionplugin kwineffects)

########################################################
# Test Screens
########################################################
set( testScreens_SRCS
    test_screens.cpp
    mock_abstract_client.cpp
    mock_client.cpp
    mock_screens.cpp
    mock_workspace.cpp
    ../screens.cpp
    ../x11eventfilter.cpp
    ../orientation_sensor.cpp
)
kconfig_add_kcfg_files(testScreens_SRCS ../settings.kcfgc)
qt5_add_dbus_adaptor( testScreens_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../org.kde.kwin.OrientationSensor.xml ${CMAKE_CURRENT_SOURCE_DIR}/../orientation_sensor.h KWin::OrientationSensor)

add_executable( testScreens ${testScreens_SRCS} ${testprintasanbase_SRCS})
target_include_directories(testScreens BEFORE PRIVATE ./)
target_link_libraries(testScreens
    Qt5::DBus
    Qt5::Sensors
    Qt5::Test
    Qt5::X11Extras
    Qt5::Widgets
    KF5::ConfigCore
    KF5::ConfigGui
    KF5::I18n
    KF5::Notifications
    KF5::WindowSystem
)

add_test(NAME kwin_testScreens COMMAND testScreens)
ecm_mark_as_test(testScreens)

########################################################
# Test XrandRScreens
########################################################
#[[set( testXRandRScreens_SRCS
     test_xrandr_screens.cpp
     mock_abstract_client.cpp
     mock_client.cpp
     mock_screens.cpp
     mock_workspace.cpp
     ../screens.cpp
     ../plugins/platforms/x11/standalone/screens_xrandr.cpp
     ../xcbutils.cpp # init of extensions
     ../x11eventfilter.cpp
     ../orientation_sensor.cpp
)
kconfig_add_kcfg_files(testXRandRScreens_SRCS ../settings.kcfgc)
qt5_add_dbus_adaptor( testXRandRScreens_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../org.kde.kwin.OrientationSensor.xml ${CMAKE_CURRENT_SOURCE_DIR}/../orientation_sensor.h KWin::OrientationSensor)
add_executable( testXRandRScreens ${testXRandRScreens_SRCS} ${testprintasanbase_SRCS})
target_link_libraries( testXRandRScreens
                       Qt5::Test
                       Qt5::DBus
                       Qt5::Gui
                       Qt5::Sensors
                       Qt5::Widgets
                       KF5::ConfigCore
                       KF5::ConfigGui
                       KF5::I18n
                       KF5::Notifications
                       KF5::WindowSystem
                       XCB::XCB
                       XCB::RANDR
                       XCB::XFIXES
                       XCB::SYNC
                       XCB::COMPOSITE
                       XCB::DAMAGE
                       XCB::GLX
                       XCB::SHM
)

#add_test(NAME kwin-testXRandRScreens COMMAND testXRandRScreens)
#ecm_mark_as_test(testXRandRScreens)
]]
########################################################
# Test ScreenEdges
########################################################
#[[set( testScreenEdges_SRCS
    test_screen_edges.cpp
    mock_abstract_client.cpp
    mock_client.cpp
    mock_screens.cpp
    mock_workspace.cpp
    ../atoms.cpp
    ../gestures.cpp
    ../screens.cpp
    ../screenedge.cpp
    ../virtualdesktops.cpp
    ../xcbutils.cpp # init of extensions
    ../plugins/platforms/x11/standalone/edge.cpp
    ../orientation_sensor.cpp
)
kconfig_add_kcfg_files(testScreenEdges_SRCS ../settings.kcfgc)
qt5_add_dbus_interface( testScreenEdges_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../org.freedesktop.ScreenSaver.xml screenlocker_interface)
qt5_add_dbus_adaptor( testScreenEdges_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../org.kde.kwin.OrientationSensor.xml ${CMAKE_CURRENT_SOURCE_DIR}/../orientation_sensor.h KWin::OrientationSensor)

add_executable( testScreenEdges ${testScreenEdges_SRCS} ${testprintasanbase_SRCS})
set_target_properties(testScreenEdges PROPERTIES COMPILE_DEFINITIONS "NO_NONE_WINDOW")
target_include_directories(testScreenEdges BEFORE PRIVATE ./)
target_link_libraries(testScreenEdges
    Qt5::DBus
    Qt5::Sensors
    Qt5::Test
    Qt5::X11Extras
    KF5::ConfigCore
    KF5::ConfigGui
    KF5::I18n
    KF5::GlobalAccel
    KF5::Notifications
    KF5::WindowSystem
    KF5::WaylandServer
    XCB::XCB
    XCB::RANDR
    XCB::XFIXES
    XCB::SYNC
    XCB::COMPOSITE
    XCB::DAMAGE
    XCB::GLX
    XCB::SHM
)

#add_test(NAME kwin_testScreenEdges COMMAND testScreenEdges)
#ecm_mark_as_test(testScreenEdges)
]]
########################################################
# Test OnScreenNotification
########################################################
set( testOnScreenNotification_SRCS
    onscreennotificationtest.cpp
    ../onscreennotification.cpp
    ../input_event_spy.cpp
)
add_executable( testOnScreenNotification ${testOnScreenNotification_SRCS} ${testprintasanbase_SRCS})

target_link_libraries(testOnScreenNotification
    Qt5::Test
    Qt5::Widgets # QAction include
    Qt5::Quick
    KF5::ConfigCore
)

add_test(NAME kwin-testOnScreenNotification COMMAND testOnScreenNotification)
ecm_mark_as_test(testOnScreenNotification)

########################################################
# Test Gestures
########################################################
set( testGestures_SRCS
    test_gestures.cpp
    ../gestures.cpp
)
add_executable( testGestures ${testGestures_SRCS} ${testprintasanbase_SRCS})

target_link_libraries(testGestures
    Qt5::Test
)

add_test(NAME kwin-testGestures COMMAND testGestures)
ecm_mark_as_test(testGestures)

########################################################
# Test X11 TimestampUpdate
########################################################
add_executable(testX11TimestampUpdate test_x11_timestamp_update.cpp ${testprintasanbase_SRCS})
target_link_libraries(testX11TimestampUpdate
    Qt5::Test
    KF5::CoreAddons
    kwin
)
add_test(NAME kwin-testX11TimestampUpdate COMMAND testX11TimestampUpdate)
ecm_mark_as_test(testX11TimestampUpdate)

set(testOpenGLContextAttributeBuilder_SRCS
    opengl_context_attribute_builder_test.cpp
    ../abstract_opengl_context_attribute_builder.cpp
    ../egl_context_attribute_builder.cpp
)

if(HAVE_EPOXY_GLX)
    set(testOpenGLContextAttributeBuilder_SRCS ${testOpenGLContextAttributeBuilder_SRCS} ../plugins/platforms/x11/standalone/glx_context_attribute_builder.cpp)
endif()
add_executable(testOpenGLContextAttributeBuilder ${testOpenGLContextAttributeBuilder_SRCS} ${testprintasanbase_SRCS})
target_link_libraries(testOpenGLContextAttributeBuilder Qt5::Test)
add_test(NAME kwin-testOpenGLContextAttributeBuilder COMMAND testOpenGLContextAttributeBuilder)
ecm_mark_as_test(testOpenGLContextAttributeBuilder)

set(testXkb_SRCS
    test_xkb.cpp
    ../xkb.cpp
)
add_executable(testXkb ${testXkb_SRCS} ${testprintasanbase_SRCS})
target_link_libraries(testXkb
    Qt5::Test
    Qt5::Gui
    Qt5::Widgets
    KF5::ConfigCore
    KF5::WindowSystem
    KF5::WaylandServer
    XKB::XKB
)
add_test(NAME kwin-testXkb COMMAND testXkb)
ecm_mark_as_test(testXkb)

if(HAVE_GBM)
    add_executable(testGbmSurface test_gbm_surface.cpp ../plugins/platforms/drm/gbm_surface.cpp ${testprintasanbase_SRCS})
    target_link_libraries(testGbmSurface Qt5::Test)
    add_test(NAME kwin-testGbmSurface COMMAND testGbmSurface)
    ecm_mark_as_test(testGbmSurface)
endif()

add_executable(testVirtualKeyboardDBus test_virtualkeyboard_dbus.cpp ../virtualkeyboard_dbus.cpp ${testprintasanbase_SRCS})
target_link_libraries(testVirtualKeyboardDBus
    Qt5::Test
    Qt5::DBus
)
add_test(NAME kwin-testVirtualKeyboardDBus COMMAND testVirtualKeyboardDBus)
ecm_mark_as_test(testVirtualKeyboardDBus)

######################################################################################################

#add_custom_target(test2
#)

#add_custom_command(TARGET test2
#    COMMAND echo " =================== CREAT LCOV REPROT BEGIN ==================== "

    #1.清理原先的gcov信息
#    COMMAND lcov --directory ./CMakeFiles/testDesktopChain.dir --zerocounters
#    COMMAND lcov --directory ./CMakeFiles/testDesktopChain.dir --zerocounters
#    COMMAND ./bin/testDesktopChain

    #2.收集gcov信息到.info文件中
#    COMMAND lcov --directory . --capture --output-file ./coverageResult/test.info

    #3.过滤一些我们不感兴趣的文件的覆盖率信息
#    COMMAND echo " =================== do filter begin ==================== "
#    COMMAND lcov --remove ./coverageResult/testDesktopChain_Coverage.info
#    '*/*_autogen/*' '*/*_autogen/*' '*/usr/include/*' '*/autotests/*' '*/googletest/*'
#    -o ./coverageResult/testDesktopChain_Coverage.info
#    COMMAND echo " =================== do filter end ==================== "

    #3.将.info信息生成报告到reprot文件夹中
#    COMMAND genhtml -o ./coverageResult/report ./coverageResult/test.info

#    COMMAND echo " =================== CREAT LCOV REPROT END ==================== "

#    COMMAND echo " Coverage files have been output to ${CMAKE_BINARY_DIR}/coverageResult/report "

#    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
#    )

#'make test'命令依赖与我们的测试程序
#add_dependencies(test2 testDesktopChain)


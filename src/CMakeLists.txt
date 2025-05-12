# for things that are also used by kwin libraries
configure_file(libkwineffects/kwinconfig.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/libkwineffects/kwinconfig.h)
configure_file(config-kwin.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kwin.h)

set(kwin_effects_dbus_xml ${CMAKE_CURRENT_SOURCE_DIR}/org.kde.kwin.Effects.xml)
if (QT_MAJOR_VERSION EQUAL "5")
    qt5_add_dbus_interface(effects_interface_SRCS ${kwin_effects_dbus_xml} kwineffects_interface)
else()
    qt_add_dbus_interface(effects_interface_SRCS ${kwin_effects_dbus_xml} kwineffects_interface)
endif()
add_library(KWinEffectsInterface STATIC ${effects_interface_SRCS})
set_property(TARGET KWinEffectsInterface PROPERTY POSITION_INDEPENDENT_CODE ON)

target_link_libraries(KWinEffectsInterface Qt${QT_MAJOR_VERSION}::DBus)

if (ECM_GLOBAL_FIND_VERSION VERSION_LESS ${KDE_COMPILERSETTINGS_LEVEL})
    set(ECM_GLOBAL_FIND_VERSION ${KF${KF_MAJOR_VERSION}_CURRENT_VERSION})
endif()
add_subdirectory(effects)
add_subdirectory(scripts)
add_subdirectory(tabbox)
add_subdirectory(scripting)
add_subdirectory(helpers)
add_subdirectory(qml)

add_subdirectory(libkwineffects)
if (KWIN_BUILD_KCMS)
    add_subdirectory(kcms)
endif()

add_library(kwin SHARED)
set_target_properties(kwin PROPERTIES
   VERSION ${PROJECT_VERSION}
   SOVERSION ${PROJECT_VERSION_MAJOR}
)

# translations
file(GLOB TS LIST_DIRECTORIES false splitscreen/translations/splitmenu*.ts)
foreach(_ts_file ${TS})
    execute_process(
        COMMAND ${LUPDATE_EXECUTABLE} -recursive ${CMAKE_SOURCE_DIR}/translations -ts ${_ts_file})
    execute_process(COMMAND ${LRELEASE_EXECUTABLE} ${_ts_file})
endforeach()

target_sources(kwin PRIVATE
    3rdparty/xcursor.c
    activation.cpp
    appmenu.cpp
    atoms.cpp
    client_machine.cpp
    colors/colordevice.cpp
    colors/colormanager.cpp
    composite.cpp
    core/colorlut.cpp
    core/colorpipelinestage.cpp
    core/colortransformation.cpp
    core/gbmloader.cpp
    core/inputbackend.cpp
    core/inputdevice.cpp
    core/output.cpp
    core/outputbackend.cpp
    core/outputconfiguration.cpp
    core/outputlayer.cpp
    core/overlaywindow.cpp
    core/renderbackend.cpp
    core/renderjournal.cpp
    core/renderlayer.cpp
    core/renderlayerdelegate.cpp
    core/renderloop.cpp
    core/rendertarget.cpp
    core/session.cpp
    core/session_consolekit.cpp
    core/session_logind.cpp
    core/session_noop.cpp
    cursor.cpp
    cursordelegate_opengl.cpp
    cursordelegate_qpainter.cpp
    cursorsource.cpp
    dbusinterface.cpp
    debug_console.cpp
    decorations/decoratedclient.cpp
    decorations/decorationbridge.cpp
    decorations/decorationpalette.cpp
    decorations/decorations_logging.cpp
    decorations/settings.cpp
    deleted.cpp
    dmabuftexture.cpp
    dpmsinputeventfilter.cpp
    effectloader.cpp
    effects.cpp
    events.cpp
    focuschain.cpp
    ftrace.cpp
    gestures.cpp
    globalshortcuts.cpp
    group.cpp
    hide_cursor_spy.cpp
    idle_inhibition.cpp
    idledetector.cpp
    input.cpp
    recordeventmonitor.cpp
    input_event.cpp
    input_event_spy.cpp
    inputmethod.cpp
    inputpanelv1integration.cpp
    inputpanelv1window.cpp
    internalwindow.cpp
    keyboard_input.cpp
    keyboard_layout.cpp
    keyboard_layout_switching.cpp
    keyboard_repeat.cpp
    killwindow.cpp
    layers.cpp
    layershellv1integration.cpp
    layershellv1window.cpp
    linux_dmabuf.cpp
    logger.cpp
    main.cpp
    modifier_only_shortcuts.cpp
    mousebuttons.cpp
    moving_client_x11_filter.cpp
    netinfo.cpp
    onscreennotification.cpp
    options.cpp
    osd.cpp
    outline.cpp
    placeholder_window.cpp
    placeholderinputeventfilter.cpp
    placeholderoutput.cpp
    placement.cpp
    placementtracker.cpp
    plugin.cpp
    pluginmanager.cpp
    pointer_input.cpp
    popup_input_filter.cpp
    rootinfo_filter.cpp
    rulebooksettings.cpp
    rules.cpp
    scene/cursoritem.cpp
    scene/cursorscene.cpp
    scene/decorationitem.cpp
    scene/dndiconitem.cpp
    scene/imageitem.cpp
    scene/item.cpp
    scene/itemrenderer.cpp
    scene/itemrenderer_opengl.cpp
    scene/itemrenderer_qpainter.cpp
    scene/itemrenderer_xrender.cpp
    scene/scene.cpp
    scene/shadowitem.cpp
    scene/surfaceitem.cpp
    scene/surfaceitem_internal.cpp
    scene/surfaceitem_wayland.cpp
    scene/surfaceitem_x11.cpp
    scene/windowitem.cpp
    scene/workspacescene.cpp
    scene/workspacescene_opengl.cpp
    scene/workspacescene_qpainter.cpp
    scene/workspacescene_xrender.cpp
    screenedge.cpp
    scripting/dbuscall.cpp
    scripting/desktopbackgrounditem.cpp
    scripting/screenedgeitem.cpp
    scripting/scriptedeffect.cpp
    scripting/scripting.cpp
    scripting/scripting_logging.cpp
    scripting/scriptingutils.cpp
    scripting/tilemodel.cpp
    scripting/v2/clientmodel.cpp
    scripting/v3/clientmodel.cpp
    scripting/v3/virtualdesktopmodel.cpp
    scripting/windowthumbnailitem.cpp
    scripting/workspace_wrapper.cpp
    shadow.cpp
    sm.cpp
    syncalarmx11filter.cpp
    tablet_input.cpp
    tabletmodemanager.cpp
    tiles/customtile.cpp
    tiles/quicktile.cpp
    tiles/tile.cpp
    tiles/tilemanager.cpp
    touch_input.cpp
    unmanaged.cpp
    useractions.cpp
    virtualdesktops.cpp
    virtualdesktopsdbustypes.cpp
    virtualkeyboard_dbus.cpp
    was_user_interaction_x11_filter.cpp
    wayland_server.cpp
    waylandshellintegration.cpp
    waylandwindow.cpp
    window.cpp
    window_property_notify_x11_filter.cpp
    workspace.cpp
    x11eventfilter.cpp
    x11syncmanager.cpp
    x11window.cpp
    xdgactivationv1.cpp
    xdgshellintegration.cpp
    xdgshellwindow.cpp
    xdgshellv6integration.cpp
    xdgshellv6window.cpp
    xkb.cpp
    xwaylandwindow.cpp
    splitscreen/splitmanage.cpp
    splitscreen/splitbar.cpp
    splitscreen/splitmenu.cpp
    splitscreen/splitscreen.qrc
    windowstyle/windowradius.cpp
    windowstyle/windowstylemanager.cpp
    windowstyle/windowshadow.cpp
    windowstyle/decorationstyle.cpp
    resources.qrc
    configreader.cpp
    dockrect.cpp
    debugpixmap.cpp
)

if (QT_MAJOR_VERSION EQUAL "5")
    pkg_check_modules(QGSETTINGS REQUIRED IMPORTED_TARGET gsettings-qt)
else()
    pkg_check_modules(QGSETTINGS REQUIRED IMPORTED_TARGET gsettings-qt6)
endif()

target_link_libraries(kwin
    kwineffects
    kwinglutils

    Qt${QT_MAJOR_VERSION}::Concurrent
    Qt${QT_MAJOR_VERSION}::DBus
    Qt${QT_MAJOR_VERSION}::Quick
    Qt${QT_MAJOR_VERSION}::Qml
    Qt${QT_MAJOR_VERSION}::Svg
    Qt${QT_MAJOR_VERSION}::Gui


    KF${KF_MAJOR_VERSION}::ConfigCore
    KF${KF_MAJOR_VERSION}::ConfigWidgets
    KF${KF_MAJOR_VERSION}::CoreAddons
    KF${KF_MAJOR_VERSION}::Crash
    KF${KF_MAJOR_VERSION}::GlobalAccel
    KF${KF_MAJOR_VERSION}::I18n
    KF${KF_MAJOR_VERSION}::Package
    KF${KF_MAJOR_VERSION}::Service
    KF${KF_MAJOR_VERSION}::WindowSystem

    XCB::COMPOSITE
    XCB::CURSOR
    XCB::DAMAGE
    XCB::GLX
    XCB::ICCCM
    XCB::KEYSYMS
    XCB::RANDR
    XCB::RENDER
    XCB::SHAPE
    XCB::SHM
    XCB::SYNC
    XCB::XCB
    XCB::XFIXES
    XCB::XINERAMA
    XCB::XTEST
    PkgConfig::XRES

    UDev::UDev
    XKB::XKB
    EGL::EGL
    epoxy::epoxy

    PkgConfig::QGSETTINGS
    systemd
    Threads::Threads
    Wayland::Server
    lcms2::lcms2
    dl
    -lXext
    -lXtst
    -ldl
)
if (QT_MAJOR_VERSION EQUAL "5")
    target_link_libraries(kwin
        Qt5::X11Extras
        Qt5XkbCommonSupport::Qt5XkbCommonSupport
        KF5::GlobalAccelPrivate
        KDecoration2::KDecoration
        KDecoration2::KDecoration2Private
    )
else()
    find_package(KGlobalAccelD REQUIRED)
    target_link_libraries(kwin
        Plasma::KWaylandClient
        K::KGlobalAccelD
        kdecorations2
        kdecorations2private
    )
    target_include_directories(kwin PRIVATE
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/kdecoration/src/private>
    )
endif()

if (KWIN_BUILD_NOTIFICATIONS)
    target_link_libraries(kwin KF${KF_MAJOR_VERSION}::Notifications)
endif()

kconfig_add_kcfg_files(kwin
    settings.kcfgc
    rulesettings.kcfgc
    rulebooksettingsbase.kcfgc
)

ki18n_wrap_ui(kwin
    debug_console.ui
    shortcutdialog.ui
)

set(kwin_dbus_SRCS)
if (QT_MAJOR_VERSION EQUAL "5")
    qt5_add_dbus_adaptor(kwin_dbus_SRCS scripting/org.kde.kwin.Script.xml scripting/scripting.h KWin::AbstractScript)
    qt5_add_dbus_adaptor(kwin_dbus_SRCS org.kde.KWin.xml dbusinterface.h KWin::DBusInterface)
    qt5_add_dbus_adaptor(kwin_dbus_SRCS org.kde.kwin.Compositing.xml dbusinterface.h KWin::CompositorDBusInterface)
    qt5_add_dbus_adaptor(kwin_dbus_SRCS ${kwin_effects_dbus_xml} effects.h KWin::EffectsHandlerImpl)
    qt5_add_dbus_adaptor(kwin_dbus_SRCS org.kde.KWin.VirtualDesktopManager.xml dbusinterface.h KWin::VirtualDesktopManagerDBusInterface)
    qt5_add_dbus_adaptor(kwin_dbus_SRCS org.kde.KWin.Session.xml sm.h KWin::SessionManager)
    qt5_add_dbus_adaptor(kwin_dbus_SRCS org.kde.KWin.Plugins.xml dbusinterface.h KWin::PluginManagerDBusInterface)
    qt5_add_dbus_adaptor(kwin_dbus_SRCS org.kde.kwin.Xkb.xml xkb.h KWin::Xkb)
else()
    qt_add_dbus_adaptor(kwin_dbus_SRCS scripting/org.kde.kwin.Script.xml scripting/scripting.h KWin::AbstractScript)
    qt_add_dbus_adaptor(kwin_dbus_SRCS org.kde.KWin.xml dbusinterface.h KWin::DBusInterface)
    qt_add_dbus_adaptor(kwin_dbus_SRCS org.kde.kwin.Compositing.xml dbusinterface.h KWin::CompositorDBusInterface)
    qt_add_dbus_adaptor(kwin_dbus_SRCS ${kwin_effects_dbus_xml} effects.h KWin::EffectsHandlerImpl)
    qt_add_dbus_adaptor(kwin_dbus_SRCS org.kde.KWin.VirtualDesktopManager.xml dbusinterface.h KWin::VirtualDesktopManagerDBusInterface)
    qt_add_dbus_adaptor(kwin_dbus_SRCS org.kde.KWin.Session.xml sm.h KWin::SessionManager)
    qt_add_dbus_adaptor(kwin_dbus_SRCS org.kde.KWin.Plugins.xml dbusinterface.h KWin::PluginManagerDBusInterface)
    qt_add_dbus_adaptor(kwin_dbus_SRCS org.kde.kwin.Xkb.xml xkb.h KWin::Xkb)
endif()

if (QT_MAJOR_VERSION EQUAL "5")
    qt5_add_dbus_interface(kwin_dbus_SRCS org.freedesktop.DBus.Properties.xml dbusproperties_interface)
else()
    qt_add_dbus_interface(kwin_dbus_SRCS org.freedesktop.DBus.Properties.xml dbusproperties_interface)
endif()

if (KWIN_BUILD_SCREENLOCKER)
if (QT_MAJOR_VERSION EQUAL "5")
    qt5_add_dbus_interface(kwin_dbus_SRCS ${KSCREENLOCKER_DBUS_INTERFACES_DIR}/kf5_org.freedesktop.ScreenSaver.xml screenlocker_interface)
    qt5_add_dbus_interface(kwin_dbus_SRCS ${KSCREENLOCKER_DBUS_INTERFACES_DIR}/org.kde.screensaver.xml kscreenlocker_interface)
else()
    qt_add_dbus_interface(kwin_dbus_SRCS ${KSCREENLOCKER_DBUS_INTERFACES_DIR}/kf6_org.freedesktop.ScreenSaver.xml screenlocker_interface)
    qt_add_dbus_interface(kwin_dbus_SRCS ${KSCREENLOCKER_DBUS_INTERFACES_DIR}/org.kde.screensaver.xml kscreenlocker_interface)
endif()
endif()

if (QT_MAJOR_VERSION EQUAL "5")
    qt5_add_dbus_interface(kwin_dbus_SRCS org.kde.kappmenu.xml appmenu_interface)
else()
    qt_add_dbus_interface(kwin_dbus_SRCS org.kde.kappmenu.xml appmenu_interface)
endif()

target_sources(kwin PRIVATE
    ${kwin_dbus_SRCS}
)

add_subdirectory(backends)
add_subdirectory(utils)
add_subdirectory(wayland)
add_subdirectory(xwayland)

if (KWIN_BUILD_ACTIVITIES)
    target_sources(kwin PRIVATE activities.cpp)
    target_link_libraries(kwin KF${KF_MAJOR_VERSION}::Activities)
endif()

if (KWIN_BUILD_SCREENLOCKER)
    target_sources(kwin PRIVATE screenlockerwatcher.cpp)
    target_link_libraries(kwin PW::KScreenLocker)
endif()

if (KWIN_BUILD_TABBOX)
    target_sources(kwin PRIVATE
        tabbox/clientmodel.cpp
        tabbox/desktopchain.cpp
        tabbox/desktopmodel.cpp
        tabbox/switcheritem.cpp
        tabbox/tabbox.cpp
        tabbox/tabbox_logging.cpp
        tabbox/tabboxconfig.cpp
        tabbox/tabboxhandler.cpp
        tabbox/x11_filter.cpp
    )
    target_link_libraries(kwin Qt${QT_MAJOR_VERSION}::GuiPrivate)
endif()

if (QT_MAJOR_VERSION EQUAL "5")
    qt5_generate_dbus_interface(virtualkeyboard_dbus.h org.kde.kwin.VirtualKeyboard.xml OPTIONS -A)
    qt5_generate_dbus_interface(tabletmodemanager.h org.kde.KWin.TabletModeManager.xml OPTIONS -A)
else()
    qt_generate_dbus_interface(virtualkeyboard_dbus.h org.kde.kwin.VirtualKeyboard.xml OPTIONS -A)
    qt_generate_dbus_interface(tabletmodemanager.h org.kde.KWin.TabletModeManager.xml OPTIONS -A)
endif()

generate_export_header(kwin EXPORT_FILE_NAME kwin_export.h)

add_executable(kwin_x11 main_x11.cpp)
target_link_libraries(kwin_x11
    KWinX11Platform
    kwin
)
kcoreaddons_target_static_plugins(kwin_x11 NAMESPACE "kwin/effects/plugins")

install(TARGETS kwin ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} LIBRARY NAMELINK_SKIP)
install(TARGETS kwin_x11 ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

add_executable(kwin_wayland main_wayland.cpp)

target_link_libraries(kwin_wayland
    kwin
    KWinXwaylandServerModule
)
kcoreaddons_target_static_plugins(kwin_wayland NAMESPACE "kwin/effects/plugins")

install(TARGETS kwin_wayland ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
if (HAVE_LIBCAP)
    install(
    CODE "execute_process(
            COMMAND
                ${SETCAP_EXECUTABLE}
                CAP_SYS_NICE=+ep
                \$ENV{DESTDIR}${CMAKE_INSTALL_FULL_BINDIR}/kwin_wayland)"
    )
endif()

add_subdirectory(platformsupport)
add_subdirectory(plugins)

target_link_libraries(kwin_wayland
    KWinQpaPlugin
    KF${KF_MAJOR_VERSION}GlobalAccelKWinPlugin
    KF${KF_MAJOR_VERSION}WindowSystemKWinPlugin
    KF${KF_MAJOR_VERSION}IdleTimeKWinPlugin
)

add_custom_target(
    KWinDBusInterfaces
    ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwin.VirtualKeyboard.xml
            ${CMAKE_CURRENT_BINARY_DIR}/org.kde.KWin.TabletModeManager.xml
)

if (PipeWire_FOUND)
    target_link_libraries(kwin_wayland KWinScreencastPlugin)
endif()

install(FILES kwin.kcfg DESTINATION ${KDE_INSTALL_KCFGDIR})
if (KWIN_BUILD_NOTIFICATIONS)
    install(FILES kwin.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})
endif()
install(
    FILES
        org.kde.KWin.VirtualDesktopManager.xml
        org.kde.KWin.xml
        org.kde.kwin.Compositing.xml
        org.kde.kwin.Effects.xml
        org.kde.KWin.Plugins.xml
        org.kde.kwin.Xkb.xml
        ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwin.VirtualKeyboard.xml
        ${CMAKE_CURRENT_BINARY_DIR}/org.kde.KWin.TabletModeManager.xml
    DESTINATION
        ${KDE_INSTALL_DBUSINTERFACEDIR}
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kwin_export.h DESTINATION ${KDE_INSTALL_INCLUDEDIR} COMPONENT Devel)

# Install the KWin/Script service type
if (KF_MAJOR_VERSION EQUAL "5")
    install(FILES scripting/kwinscript.desktop DESTINATION ${KDE_INSTALL_KSERVICETYPESDIR})
endif()

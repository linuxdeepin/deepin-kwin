########### next target ###############
# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kcmkwm\")

set(kcm_kwinoptions_PART_SRCS
    main.cpp
    mouse.cpp
    windows.cpp
)

ki18n_wrap_ui(kcm_kwinoptions_PART_SRCS
    actions.ui
    advanced.ui
    focus.ui
    mouse.ui
    moving.ui
)

kcmutils_generate_module_data(
    kcm_kwinoptions_PART_SRCS
    MODULE_DATA_HEADER kwinoptionsdata.h
    MODULE_DATA_CLASS_NAME KWinOptionsData
    SETTINGS_HEADERS kwinoptions_settings.h kwinoptions_kdeglobals_settings.h
    SETTINGS_CLASSES KWinOptionsSettings KWinOptionsKDEGlobalsSettings
)

kconfig_add_kcfg_files(kcm_kwinoptions_PART_SRCS kwinoptions_settings.kcfgc GENERATE_MOC)
kconfig_add_kcfg_files(kcm_kwinoptions_PART_SRCS kwinoptions_kdeglobals_settings.kcfgc GENERATE_MOC)

if (QT_MAJOR_VERSION EQUAL "5")
    qt5_add_dbus_interface(kcm_kwinoptions_PART_SRCS ${KWin_SOURCE_DIR}/src/org.kde.kwin.Effects.xml kwin_effects_interface)
else()
    qt_add_dbus_interface(kcm_kwinoptions_PART_SRCS ${KWin_SOURCE_DIR}/src/org.kde.kwin.Effects.xml kwin_effects_interface)
endif()

kcoreaddons_add_plugin(kcm_kwinoptions SOURCES ${kcm_kwinoptions_PART_SRCS} INSTALL_NAMESPACE "plasma/kcms/systemsettings_qwidgets")
target_link_libraries(kcm_kwinoptions kwin Qt${QT_MAJOR_VERSION}::DBus KF${KF_MAJOR_VERSION}::KCMUtils KF${KF_MAJOR_VERSION}::I18n KF${KF_MAJOR_VERSION}::ConfigWidgets KF${KF_MAJOR_VERSION}::Service KF${KF_MAJOR_VERSION}::WindowSystem)

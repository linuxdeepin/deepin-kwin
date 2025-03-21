#########################################################################
# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kcmkwincompositing\")

################# configure checks and create the configured files #################

set(kwincompositing_SRC
    main.cpp
    kwincompositingdata.cpp
)

kconfig_add_kcfg_files(kwincompositing_SRC kwincompositing_setting.kcfgc GENERATE_MOC)

if (QT_MAJOR_VERSION EQUAL "5")
qt5_add_dbus_interface(kwincompositing_SRC
    ${KWin_SOURCE_DIR}/src/org.kde.kwin.Compositing.xml kwin_compositing_interface
)
else()
qt_add_dbus_interface(kwincompositing_SRC
    ${KWin_SOURCE_DIR}/src/org.kde.kwin.Compositing.xml kwin_compositing_interface
)
endif()

ki18n_wrap_ui(kwincompositing_SRC compositing.ui)

kcoreaddons_add_plugin(kwincompositing SOURCES ${kwincompositing_SRC} INSTALL_NAMESPACE "plasma/kcms/systemsettings_qwidgets")

target_link_libraries(kwincompositing
    Qt${QT_MAJOR_VERSION}::DBus
    Qt${QT_MAJOR_VERSION}::Widgets

    KF${KF_MAJOR_VERSION}::ConfigCore
    KF${KF_MAJOR_VERSION}::CoreAddons
    KF${KF_MAJOR_VERSION}::I18n
    KF${KF_MAJOR_VERSION}::KCMUtils
    KF${KF_MAJOR_VERSION}::WindowSystem
    KF${KF_MAJOR_VERSION}::Service
)

# KI18N Translation Domain for this library.
add_definitions(-DTRANSLATION_DOMAIN=\"kcm_kwin_virtualdesktops\")

########### next target ###############

set(kcm_kwin_virtualdesktops_PART_SRCS
    ../../virtualdesktopsdbustypes.cpp
    animationsmodel.cpp
    desktopsmodel.cpp
    virtualdesktops.cpp
    virtualdesktopsdata.cpp
)

kconfig_add_kcfg_files(kcm_kwin_virtualdesktops_PART_SRCS virtualdesktopssettings.kcfgc GENERATE_MOC)

kcoreaddons_add_plugin(kcm_kwin_virtualdesktops SOURCES ${kcm_kwin_virtualdesktops_PART_SRCS} INSTALL_NAMESPACE "plasma/kcms/systemsettings")

target_link_libraries(kcm_kwin_virtualdesktops
    Qt${QT_MAJOR_VERSION}::DBus

    KF${KF_MAJOR_VERSION}::I18n
    KF${KF_MAJOR_VERSION}::KCMUtils
    KF${KF_MAJOR_VERSION}::QuickAddons
    KF${KF_MAJOR_VERSION}::XmlGui

    kcmkwincommon
)

########### install files ###############

install(FILES virtualdesktopssettings.kcfg DESTINATION ${KDE_INSTALL_KCFGDIR})
kpackage_install_package(package kcm_kwin_virtualdesktops kcms)

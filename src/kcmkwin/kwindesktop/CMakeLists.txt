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

add_library(deepin-kcm_kwin_virtualdesktops MODULE ${kcm_kwin_virtualdesktops_PART_SRCS})

target_link_libraries(deepin-kcm_kwin_virtualdesktops
    Qt::DBus

    KF5::I18n
    KF5::KCMUtils
    KF5::QuickAddons
    KF5::XmlGui

    deepin-kcmkwincommon
)

kcoreaddons_desktop_to_json(deepin-kcm_kwin_virtualdesktops "deepin-kcm_kwin_virtualdesktops.desktop")

########### install files ###############

install(FILES deepin-virtualdesktopssettings.kcfg DESTINATION ${KDE_INSTALL_KCFGDIR})
install(TARGETS deepin-kcm_kwin_virtualdesktops DESTINATION ${KDE_INSTALL_PLUGINDIR}/kcms)
install(FILES deepin-kcm_kwin_virtualdesktops.desktop  DESTINATION  ${KDE_INSTALL_KSERVICES5DIR})
kpackage_install_package(package deepin-kcm_kwin_virtualdesktops kcms)

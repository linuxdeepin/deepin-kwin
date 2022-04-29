# KI18N Translation Domain for this library.
add_definitions(-DTRANSLATION_DOMAIN=\"kcm_kwin_effects\")

########### next target ###############

set(kcm_kwin_effects_PART_SRCS
    kcm.cpp
    effectsfilterproxymodel.cpp
    desktopeffectsdata.cpp
)

add_library(deepin-kcm_kwin_effects MODULE ${kcm_kwin_effects_PART_SRCS})

target_link_libraries(deepin-kcm_kwin_effects
    Qt::DBus

    KF5::KCMUtils
    KF5::I18n
    KF5::KCMUtils
    KF5::NewStuff
    KF5::QuickAddons
    KF5::XmlGui

    deepin-kcmkwincommon
)

kcoreaddons_desktop_to_json(deepin-kcm_kwin_effects "deepin-kcm_kwin_effects.desktop")

########### install files ###############

install(TARGETS deepin-kcm_kwin_effects DESTINATION ${KDE_INSTALL_PLUGINDIR}/kcms)
install(FILES deepin-kcm_kwin_effects.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR})
install(FILES deepin-kwineffect.knsrc DESTINATION ${KDE_INSTALL_KNSRCDIR})
kpackage_install_package(package deepin-kcm_kwin_effects kcms)

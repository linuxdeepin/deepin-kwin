# KI18N Translation Domain for this library.
add_definitions(-DTRANSLATION_DOMAIN=\"kcm_kwin_effects\")

########### next target ###############

set(kcm_kwin_effects_PART_SRCS
    kcm.cpp
    effectsfilterproxymodel.cpp
    desktopeffectsdata.cpp
)

kcoreaddons_add_plugin(kcm_kwin_effects SOURCES ${kcm_kwin_effects_PART_SRCS} INSTALL_NAMESPACE "plasma/kcms/systemsettings")

target_link_libraries(kcm_kwin_effects
    Qt${QT_MAJOR_VERSION}::DBus

    KF${KF_MAJOR_VERSION}::KCMUtils
    KF${KF_MAJOR_VERSION}::I18n
    KF${KF_MAJOR_VERSION}::KCMUtils
    KF${KF_MAJOR_VERSION}::NewStuff
    KF${KF_MAJOR_VERSION}::QuickAddons
    KF${KF_MAJOR_VERSION}::XmlGui

    kcmkwincommon
)

########### install files ###############

install(FILES kwineffect.knsrc DESTINATION ${KDE_INSTALL_KNSRCDIR})
kpackage_install_package(package kcm_kwin_effects kcms)

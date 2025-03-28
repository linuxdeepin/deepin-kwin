# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kcm_kwin_scripts\")

kcoreaddons_add_plugin(kcm_kwin_scripts INSTALL_NAMESPACE plasma/kcms/systemsettings)

target_sources(kcm_kwin_scripts PRIVATE
    module.cpp
    kwinscriptsdata.cpp
)

target_link_libraries(kcm_kwin_scripts
    Qt${QT_MAJOR_VERSION}::DBus

    KF${KF_MAJOR_VERSION}::I18n
    KF${KF_MAJOR_VERSION}::KCMUtilsCore
    KF${KF_MAJOR_VERSION}::KCMUtils
    KF${KF_MAJOR_VERSION}::NewStuffWidgets
    KF${KF_MAJOR_VERSION}::Package
    KF${KF_MAJOR_VERSION}::QuickAddons
    KF${KF_MAJOR_VERSION}::ItemViews
)

kpackage_install_package(package kcm_kwin_scripts kcms)
install(FILES kwinscripts.knsrc DESTINATION ${KDE_INSTALL_KNSRCDIR})

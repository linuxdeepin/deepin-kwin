set(plugin_SRCS
    previewbutton.cpp
    previewbridge.cpp
    previewclient.cpp
    previewitem.cpp
    previewsettings.cpp
    plugin.cpp
    buttonsmodel.cpp
    ../../../decorations/decorationpalette.cpp
    ../../../decorations/decorations_logging.cpp
)

add_library(kdecorationprivatedeclarative SHARED ${plugin_SRCS})
target_link_libraries(kdecorationprivatedeclarative
    KDecoration2::KDecoration
    KDecoration2::KDecoration2Private
    Qt${QT_MAJOR_VERSION}::DBus
    Qt${QT_MAJOR_VERSION}::Quick
    KF${KF_MAJOR_VERSION}::CoreAddons
    KF${KF_MAJOR_VERSION}::ConfigWidgets
    KF${KF_MAJOR_VERSION}::I18n
    KF${KF_MAJOR_VERSION}::Service
)

install(TARGETS kdecorationprivatedeclarative DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/kwin/private/kdecoration )
install(FILES qmldir DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/kwin/private/kdecoration )

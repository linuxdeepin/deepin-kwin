#######################################
# Effect

# Source files
set(multitaskview_SOURCES
    multitaskview.cpp
    multitaskview.qrc
    main.cpp
)
kwin4_add_effect_module(kwin4_effect_multitaskview ${multitaskview_SOURCES})
target_link_libraries(kwin4_effect_multitaskview PRIVATE
    kwineffects
    kwinglutils
    KF5::GlobalAccel
    Qt5::DBus
    Qt5::X11Extras
    Qt5::Concurrent
    PkgConfig::QGSETTINGS
)

install(DIRECTORY qml DESTINATION ${KDE_INSTALL_DATADIR}/kwin/effects/multitaskview)
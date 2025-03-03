#######################################
# Effect

set(splitscreen_SOURCES
    main.cpp
    splitprevieweffect.cpp
)

find_package(Qt${QT_MAJOR_VERSION} CONFIG REQUIRED COMPONENTS
    QuickWidgets
)

kwin4_add_effect_module(kwin4_effect_splitscreen ${splitscreen_SOURCES})
target_link_libraries(kwin4_effect_splitscreen PRIVATE
    kwineffects
    Qt${QT_MAJOR_VERSION}::QuickWidgets
    Qt${QT_MAJOR_VERSION}::DBus
    #Qt${QT_MAJOR_VERSION}::X11Extras
    KF6::ConfigGui
)

install(DIRECTORY qml DESTINATION ${KDE_INSTALL_DATADIR}/kwin/effects/splitscreen)


#######################################
# Effect

set(splitscreen_SOURCES
    main.cpp
    splitprevieweffect.cpp
)

find_package(Qt${QT_MAJOR_VERSION} CONFIG REQUIRED COMPONENTS
    QuickWidgets
)

kwin4_add_effect_module(splitscreen ${splitscreen_SOURCES})
target_link_libraries(splitscreen PRIVATE
    kwineffects
    Qt${QT_MAJOR_VERSION}::QuickWidgets
    Qt${QT_MAJOR_VERSION}::DBus
    KF${KF_MAJOR_VERSION}::ConfigGui
)

if (QT_MAJOR_VERSION EQUAL "5")
    target_link_libraries(splitscreen PRIVATE Qt5::X11Extras)
else()
    target_link_libraries(splitscreen PRIVATE Qt6::GuiPrivate)
endif()


install(DIRECTORY qml DESTINATION ${KDE_INSTALL_DATADIR}/kwin/effects/splitscreen)


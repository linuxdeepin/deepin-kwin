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
    KF${KF_MAJOR_VERSION}::ConfigGui
)
set_target_properties(kwin4_effect_splitscreen PROPERTIES
    LIBRARY_OUTPUT_NAME "splitscreen"
)

if (QT_MAJOR_VERSION EQUAL "5")
    target_link_libraries(kwin4_effect_splitscreen PRIVATE Qt5::X11Extras)
else()
    target_link_libraries(kwin4_effect_splitscreen PRIVATE Qt6::GuiPrivate)
endif()


install(DIRECTORY qml DESTINATION ${KDE_INSTALL_DATADIR}/kwin/effects/splitscreen)


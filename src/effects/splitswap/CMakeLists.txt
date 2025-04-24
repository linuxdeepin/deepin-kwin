#######################################
# Effect

set(splitswap_SOURCES
    main.cpp
    splitswap.cpp
)

find_package(Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    QuickWidgets
)

kwin4_add_effect_module(splitswap ${splitswap_SOURCES})
target_link_libraries(splitswap PRIVATE
    kwineffects
    Qt${QT_MAJOR_VERSION}::QuickWidgets
    Qt${QT_MAJOR_VERSION}::DBus
    KF${KF_MAJOR_VERSION}::ConfigGui
)

if (QT_MAJOR_VERSION EQUAL "5")
    target_link_libraries(splitswap PRIVATE Qt5::X11Extras)
else()
    target_link_libraries(splitswap PRIVATE Qt6::GuiPrivate)
endif()

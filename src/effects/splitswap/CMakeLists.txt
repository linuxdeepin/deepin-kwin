#######################################
# Effect

set(splitswap_SOURCES
    main.cpp
    splitswap.cpp
)

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    QuickWidgets
)

kwin4_add_effect_module(kwin4_effect_splitswap ${splitswap_SOURCES})
target_link_libraries(kwin4_effect_splitswap PRIVATE
    kwineffects
    Qt${QT_MAJOR_VERSION}::QuickWidgets
    Qt6::DBus
    #Qt6::X11Extras
    KF6::ConfigGui
)



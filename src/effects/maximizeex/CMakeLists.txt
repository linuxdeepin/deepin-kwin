#######################################
# Effect

set(maximize_SOURCES
    main.cpp
    maximizeeffect.cpp
)

find_package(Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
   QuickWidgets
)

kwin4_add_effect_module(maximizeex ${maximize_SOURCES})
target_link_libraries(maximizeex PRIVATE
    kwineffects
    Qt${QT_MAJOR_VERSION}::QuickWidgets
    Qt${QT_MAJOR_VERSION}::DBus
    KF${KF_MAJOR_VERSION}::ConfigGui
)

if (QT_MAJOR_VERSION EQUAL "5")
    target_link_libraries(maximizeex PRIVATE Qt5::X11Extras)
endif()

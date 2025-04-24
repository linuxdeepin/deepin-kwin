#######################################
# Effect

# Source files
set(screenedge_SOURCES
    main.cpp
    screenedgeeffect.cpp
)

kwin4_add_effect_module(screenedge ${screenedge_SOURCES})
target_link_libraries(screenedge PRIVATE
    kwineffects
    kwinglutils
)

if (QT_MAJOR_VERSION EQUAL "5")
    target_link_libraries(screenedge PRIVATE KF5::Plasma)
else()
    find_package(KF6 REQUIRED COMPONENTS Svg)
    target_link_libraries(screenedge PRIVATE KF6::Svg)
endif()
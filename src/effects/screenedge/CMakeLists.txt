#######################################
# Effect

# Source files
set(screenedge_SOURCES
    main.cpp
    screenedgeeffect.cpp
)

find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS Svg)

kwin4_add_effect_module(kwin4_effect_screenedge ${screenedge_SOURCES})
target_link_libraries(kwin4_effect_screenedge PRIVATE
    kwineffects
    kwinglutils

    KF6::Svg
)

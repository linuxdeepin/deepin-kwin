#######################################
# Effect

set(touchpoints_SOURCES
    main.cpp
    touchpoints.cpp
)

kwin4_add_effect_module(kwin4_effect_touchpoints ${touchpoints_SOURCES})
target_link_libraries(kwin4_effect_touchpoints PRIVATE
    kwineffects
    kwinglutils

    KF${KF_MAJOR_VERSION}::GlobalAccel
)
set_target_properties(kwin4_effect_touchpoints PROPERTIES
    LIBRARY_OUTPUT_NAME "touchpoints"
)

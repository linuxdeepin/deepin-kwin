#######################################
# Effect

set(screentransform_SOURCES
    main.cpp
    screentransform.cpp
    screentransform.qrc
)

kwin4_add_effect_module(kwin4_effect_screentransform ${screentransform_SOURCES})
target_link_libraries(kwin4_effect_screentransform PRIVATE
    kwineffects
    kwinglutils
)
set_target_properties(kwin4_effect_screentransform PROPERTIES
    LIBRARY_OUTPUT_NAME "screentransform"
)

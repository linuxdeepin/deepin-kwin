#######################################
# Effect

set(touchecho_SOURCES
    main.cpp
    touchecho.cpp
)

kwin4_add_effect_module(kwin4_effect_touchecho ${touchecho_SOURCES})
target_link_libraries(kwin4_effect_touchecho PRIVATE
    kwineffects
    kwinglutils

    KF5::GlobalAccel
)

#######################################
# Effect

# Source files
set(kscreen_SOURCES
    kscreen.cpp
    main.cpp
)

kconfig_add_kcfg_files(kscreen_SOURCES kscreenconfig.kcfgc)
kwin4_add_effect_module(kscreen ${kscreen_SOURCES})
target_link_libraries(kscreen PRIVATE
    kwineffects

    KF${KF_MAJOR_VERSION}::ConfigGui
)

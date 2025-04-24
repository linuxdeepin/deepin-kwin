#######################################
# Effect

set(glide_SOURCES
    glide.cpp
    main.cpp
)

kconfig_add_kcfg_files(glide_SOURCES
    glideconfig.kcfgc
)

kwin4_add_effect_module(glide ${glide_SOURCES})
target_link_libraries(glide PRIVATE
    kwineffects

    KF${KF_MAJOR_VERSION}::ConfigGui
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_glide_config_SRCS glide_config.cpp)
    ki18n_wrap_ui(kwin_glide_config_SRCS glide_config.ui)
    kconfig_add_kcfg_files(kwin_glide_config_SRCS glideconfig.kcfgc)

    kwin_add_effect_config(kwin_glide_config ${kwin_glide_config_SRCS})

    target_link_libraries(kwin_glide_config
        KF${KF_MAJOR_VERSION}::ConfigWidgets
        KF${KF_MAJOR_VERSION}::CoreAddons
        KF${KF_MAJOR_VERSION}::I18n
        KF${KF_MAJOR_VERSION}::KCMUtils
        Qt${QT_MAJOR_VERSION}::DBus
        KWinEffectsInterface
    )
endif()

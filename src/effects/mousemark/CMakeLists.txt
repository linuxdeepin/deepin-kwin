#######################################
# Effect

set(mousemark_SOURCES
    main.cpp
    mousemark.cpp
)

kconfig_add_kcfg_files(mousemark_SOURCES
    mousemarkconfig.kcfgc
)

kwin4_add_effect_module(mousemark ${mousemark_SOURCES})
target_link_libraries(mousemark PRIVATE
    kwineffects
    kwinglutils

    KF${KF_MAJOR_VERSION}::ConfigGui
    KF${KF_MAJOR_VERSION}::GlobalAccel
    KF${KF_MAJOR_VERSION}::I18n
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_mousemark_config_SRCS mousemark_config.cpp)
    ki18n_wrap_ui(kwin_mousemark_config_SRCS mousemark_config.ui)
    kconfig_add_kcfg_files(kwin_mousemark_config_SRCS mousemarkconfig.kcfgc)

    kwin_add_effect_config(kwin_mousemark_config ${kwin_mousemark_config_SRCS})

    target_link_libraries(kwin_mousemark_config
        KF${KF_MAJOR_VERSION}::ConfigWidgets
        KF${KF_MAJOR_VERSION}::CoreAddons
        KF${KF_MAJOR_VERSION}::GlobalAccel
        KF${KF_MAJOR_VERSION}::I18n
        KF${KF_MAJOR_VERSION}::XmlGui
        KWinEffectsInterface
    )
endif()

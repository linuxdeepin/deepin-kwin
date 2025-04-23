#######################################
# Effect

set(magiclamp_SOURCES
    magiclamp.cpp
    main.cpp
)

kconfig_add_kcfg_files(magiclamp_SOURCES
    magiclampconfig.kcfgc
)

kwin4_add_effect_module(magiclamp ${magiclamp_SOURCES})

target_link_libraries(magiclamp PRIVATE
    kwineffects

    KF${KF_MAJOR_VERSION}::ConfigGui
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_magiclamp_config_SRCS magiclamp_config.cpp)
    ki18n_wrap_ui(kwin_magiclamp_config_SRCS magiclamp_config.ui)
    kconfig_add_kcfg_files(kwin_magiclamp_config_SRCS magiclampconfig.kcfgc)

    kwin_add_effect_config(kwin_magiclamp_config ${kwin_magiclamp_config_SRCS})

    target_link_libraries(kwin_magiclamp_config
        KF${KF_MAJOR_VERSION}::ConfigWidgets
        KF${KF_MAJOR_VERSION}::CoreAddons
        KF${KF_MAJOR_VERSION}::I18n
        KF${KF_MAJOR_VERSION}::KCMUtils

        Qt${QT_MAJOR_VERSION}::DBus
        KWinEffectsInterface
    )
endif()

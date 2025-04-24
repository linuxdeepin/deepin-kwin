#######################################
# Effect

set(diminactive_SOURCES
    diminactive.cpp
    main.cpp
)

kconfig_add_kcfg_files(diminactive_SOURCES
    diminactiveconfig.kcfgc
)

kwin4_add_effect_module(diminactive ${diminactive_SOURCES})
target_link_libraries(diminactive PRIVATE
    kwineffects

    KF${KF_MAJOR_VERSION}::ConfigGui
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_diminactive_config_SRCS diminactive_config.cpp)
    ki18n_wrap_ui(kwin_diminactive_config_SRCS diminactive_config.ui)
    kconfig_add_kcfg_files(kwin_diminactive_config_SRCS diminactiveconfig.kcfgc)

    kwin_add_effect_config(kwin_diminactive_config ${kwin_diminactive_config_SRCS})

    target_link_libraries(kwin_diminactive_config
        KF${KF_MAJOR_VERSION}::ConfigWidgets
        KF${KF_MAJOR_VERSION}::CoreAddons
        KF${KF_MAJOR_VERSION}::I18n
        KF${KF_MAJOR_VERSION}::KCMUtils
        Qt${QT_MAJOR_VERSION}::DBus
        KWinEffectsInterface
    )
endif()

#######################################
# Effect

set(blur_SOURCES
    blur.cpp
    blur.qrc
    main.cpp
)

kconfig_add_kcfg_files(blur_SOURCES
    blurconfig.kcfgc
)

kwin4_add_effect_module(blur ${blur_SOURCES})
target_link_libraries(blur PRIVATE
    kwin
    kwineffects
    kwinglutils
    KF${KF_MAJOR_VERSION}::ConfigGui
    KDecoration2::KDecoration
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_blur_config_SRCS blur_config.cpp)
    ki18n_wrap_ui(kwin_blur_config_SRCS blur_config.ui)
    kconfig_add_kcfg_files(kwin_blur_config_SRCS blurconfig.kcfgc)

    kwin_add_effect_config(kwin_blur_config ${kwin_blur_config_SRCS})

    target_link_libraries(kwin_blur_config
        KF${KF_MAJOR_VERSION}::ConfigWidgets
        KF${KF_MAJOR_VERSION}::CoreAddons
        KF${KF_MAJOR_VERSION}::I18n
        KF${KF_MAJOR_VERSION}::KCMUtils

        Qt${QT_MAJOR_VERSION}::DBus
        KWinEffectsInterface
    )
endif()

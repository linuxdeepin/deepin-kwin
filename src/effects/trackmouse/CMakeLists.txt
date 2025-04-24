#######################################
# Effect
# Data files
install(FILES data/tm_inner.png data/tm_outer.png DESTINATION ${KDE_INSTALL_DATADIR}/kwin)

set(trackmouse_SOURCES
    main.cpp
    trackmouse.cpp
)

kconfig_add_kcfg_files(trackmouse_SOURCES
    trackmouseconfig.kcfgc
)

kwin4_add_effect_module(trackmouse ${trackmouse_SOURCES})
target_link_libraries(trackmouse PRIVATE
    kwineffects
    kwinglutils

    KF${KF_MAJOR_VERSION}::ConfigGui
    KF${KF_MAJOR_VERSION}::GlobalAccel
    KF${KF_MAJOR_VERSION}::I18n
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_trackmouse_config_SRCS trackmouse_config.cpp)
    ki18n_wrap_ui(kwin_trackmouse_config_SRCS trackmouse_config.ui)
    kconfig_add_kcfg_files(kwin_trackmouse_config_SRCS trackmouseconfig.kcfgc)

    kwin_add_effect_config(kwin_trackmouse_config ${kwin_trackmouse_config_SRCS})

    target_link_libraries(kwin_trackmouse_config
        KF${KF_MAJOR_VERSION}::ConfigWidgets
        KF${KF_MAJOR_VERSION}::CoreAddons
        KF${KF_MAJOR_VERSION}::GlobalAccel
        KF${KF_MAJOR_VERSION}::I18n
        KF${KF_MAJOR_VERSION}::XmlGui
        KWinEffectsInterface
    )
endif()

#######################################
# Effect

set(showpaint_SOURCES
    main.cpp
    showpaint.cpp
)

kwin4_add_effect_module(showpaint ${showpaint_SOURCES})
target_link_libraries(showpaint PRIVATE
    kwineffects
    kwinglutils

    KF${KF_MAJOR_VERSION}::GlobalAccel
    KF${KF_MAJOR_VERSION}::I18n
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_showpaint_config_SRCS showpaint_config.cpp)
    ki18n_wrap_ui(kwin_showpaint_config_SRCS showpaint_config.ui)

    kwin_add_effect_config(kwin_showpaint_config ${kwin_showpaint_config_SRCS})

    target_link_libraries(kwin_showpaint_config
        KF${KF_MAJOR_VERSION}::ConfigWidgets
        KF${KF_MAJOR_VERSION}::CoreAddons
        KF${KF_MAJOR_VERSION}::GlobalAccel
        KF${KF_MAJOR_VERSION}::I18n
        KF${KF_MAJOR_VERSION}::XmlGui
        KF${KF_MAJOR_VERSION}::KCMUtils
    )
endif()

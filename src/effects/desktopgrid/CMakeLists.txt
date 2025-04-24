# SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause

set(desktopgrid_SOURCES
    main.cpp
    desktopgrideffect.cpp
)

kconfig_add_kcfg_files(desktopgrid_SOURCES
    desktopgridconfig.kcfgc
)


kwin4_add_effect_module(desktopgrid ${desktopgrid_SOURCES})

target_link_libraries(desktopgrid PRIVATE
    kwineffects

    KF${KF_MAJOR_VERSION}::ConfigGui
    KF${KF_MAJOR_VERSION}::GlobalAccel
    KF${KF_MAJOR_VERSION}::I18n

    Qt${QT_MAJOR_VERSION}::Quick
    )

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_desktopgrid_config_SRCS desktopgrid_config.cpp)
    ki18n_wrap_ui(kwin_desktopgrid_config_SRCS desktopgrid_config.ui)
    kconfig_add_kcfg_files(kwin_desktopgrid_config_SRCS desktopgridconfig.kcfgc)

    kwin_add_effect_config(kwin_desktopgrid_config ${kwin_desktopgrid_config_SRCS})

    target_link_libraries(kwin_desktopgrid_config
        KF${KF_MAJOR_VERSION}::ConfigWidgets
        KF${KF_MAJOR_VERSION}::CoreAddons
        KF${KF_MAJOR_VERSION}::GlobalAccel
        KF${KF_MAJOR_VERSION}::I18n
        KF${KF_MAJOR_VERSION}::XmlGui
        Qt${QT_MAJOR_VERSION}::Quick
        kwineffects
        KWinEffectsInterface
    )
endif()


install(DIRECTORY qml DESTINATION ${KDE_INSTALL_DATADIR}/kwin/effects/desktopgrid)

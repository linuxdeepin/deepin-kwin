#SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalzez <aleixpol@kde.org>
#SPDX-License-Identifier: BSD-3-Clause

add_definitions(-DTRANSLATION_DOMAIN=\"kcm_kwinxwayland\")

kcmutils_generate_module_data(
    kcm_kwinxwayland_PART_SRCS
    MODULE_DATA_HEADER kwinxwaylanddata.h
    MODULE_DATA_CLASS_NAME KWinXwaylandData
    SETTINGS_HEADERS kwinxwaylandsettings.h
    SETTINGS_CLASSES KWinXwaylandSettings
)

kconfig_add_kcfg_files(kcm_kwinxwayland_PART_SRCS kwinxwaylandsettings.kcfgc GENERATE_MOC)
kcoreaddons_add_plugin(kcm_kwinxwayland SOURCES kcmkwinxwayland.cpp ${kcm_kwinxwayland_PART_SRCS} INSTALL_NAMESPACE plasma/kcms/systemsettings)
target_include_directories(kcm_kwinxwayland PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(kcm_kwinxwayland
    KF${KF_MAJOR_VERSION}::I18n
    KF${KF_MAJOR_VERSION}::KCMUtils
    KF${KF_MAJOR_VERSION}::QuickAddons
    Wayland::Client
)
kpackage_install_package(package kcm_kwinxwayland kcms)

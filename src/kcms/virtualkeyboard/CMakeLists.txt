#SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalzez <aleixpol@kde.org>
#SPDX-License-Identifier: BSD-3-Clause

add_definitions(-DTRANSLATION_DOMAIN=\"kcm_virtualkeyboard\")

kcmutils_generate_module_data(
    kcm_virtualkeyboard_PART_SRCS
    MODULE_DATA_HEADER virtualkeyboarddata.h
    MODULE_DATA_CLASS_NAME VirtualKeyboardData
    SETTINGS_HEADERS virtualkeyboardsettings.h
    SETTINGS_CLASSES VirtualKeyboardSettings
)

kconfig_add_kcfg_files(kcm_virtualkeyboard_PART_SRCS virtualkeyboardsettings.kcfgc GENERATE_MOC)
kcoreaddons_add_plugin(kcm_virtualkeyboard SOURCES kcmvirtualkeyboard.cpp ${kcm_virtualkeyboard_PART_SRCS} INSTALL_NAMESPACE plasma/kcms/systemsettings)
target_include_directories(kcm_virtualkeyboard PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(kcm_virtualkeyboard
    KF${KF_MAJOR_VERSION}::I18n
    KF${KF_MAJOR_VERSION}::KCMUtils
    KF${KF_MAJOR_VERSION}::QuickAddons
)

kpackage_install_package(package kcm_virtualkeyboard kcms)

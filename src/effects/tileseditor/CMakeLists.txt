# SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause

if (KWIN_BUILD_KCMS)
    add_subdirectory(kcm)
endif()

set(tileseditor_SOURCES
    main.cpp
    tileseditoreffect.cpp
)

kwin4_add_effect_module(tileseditor ${tileseditor_SOURCES})

target_link_libraries(tileseditor PRIVATE
    kwineffects

    KF${KF_MAJOR_VERSION}::ConfigGui
    KF${KF_MAJOR_VERSION}::GlobalAccel
    KF${KF_MAJOR_VERSION}::I18n

    Qt${QT_MAJOR_VERSION}::Quick
    )

install(DIRECTORY qml DESTINATION ${KDE_INSTALL_DATADIR}/kwin/effects/tileseditor)

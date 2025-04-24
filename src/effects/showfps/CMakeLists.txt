# SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>
#
# SPDX-License-Identifier: BSD-3-Clause

set(showfps_SOURCES
    main.cpp
    showfpseffect.cpp
)

kwin4_add_effect_module(showfps ${showfps_SOURCES})

target_link_libraries(showfps PRIVATE
    kwineffects

    KF${KF_MAJOR_VERSION}::I18n

    Qt${QT_MAJOR_VERSION}::Quick
    )

install(DIRECTORY qml DESTINATION ${KDE_INSTALL_DATADIR}/kwin/effects/showfps)

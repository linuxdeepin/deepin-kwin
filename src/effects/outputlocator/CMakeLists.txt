# SPDX-FileCopyrightText: 2022 David Redondo <kde@david-redondo.de>
#
# SPDX-License-Identifier: BSD-3-Clause


kwin4_add_effect_module(outputlocator main.cpp outputlocator.cpp)
target_link_libraries(outputlocator PRIVATE
    kwineffects
    Qt${QT_MAJOR_VERSION}::DBus
    Qt${QT_MAJOR_VERSION}::Quick
    KF${KF_MAJOR_VERSION}::I18n
)

install(DIRECTORY qml DESTINATION ${KDE_INSTALL_DATADIR}/kwin/effects/outputlocator)

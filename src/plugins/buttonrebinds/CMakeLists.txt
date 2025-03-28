# SPDX-FileCopyrightText: 2022 David Redondo <kde@david-redono.de>
# SPDX-License-Identifier: BSD-3-Clause

kcoreaddons_add_plugin(MouseButtonToKeyPlugin INSTALL_NAMESPACE "kwin/plugins")

ecm_qt_declare_logging_category(MouseButtonToKeyPlugin
    HEADER buttonrebinds_debug.h
    IDENTIFIER KWIN_BUTTONREBINDS
    CATEGORY_NAME kwin_buttonrebinds
    DEFAULT_SEVERITY Warning
)

target_sources(MouseButtonToKeyPlugin PRIVATE
    main.cpp
    buttonrebindsfilter.cpp
)

set_target_properties(MouseButtonToKeyPlugin PROPERTIES
    LIBRARY_OUTPUT_NAME "kwin5_plugin_buttonrebinds"
)

target_link_libraries(MouseButtonToKeyPlugin PRIVATE kwin KF${KF_MAJOR_VERSION}::WindowSystem)


add_subdirectory(v2)

if (KWIN_BUILD_KCMS)
    set(kcm_kwin4_genericscripted_SRCS genericscriptedconfig.cpp)
    if (QT_MAJOR_VERSION EQUAL "5")
        qt5_add_dbus_interface(kcm_kwin4_genericscripted_SRCS ${kwin_effects_dbus_xml} kwineffects_interface)
    else()
        qt_add_dbus_interface(kcm_kwin4_genericscripted_SRCS ${kwin_effects_dbus_xml} kwineffects_interface)
    endif()
    add_library(kcm_kwin4_genericscripted MODULE ${kcm_kwin4_genericscripted_SRCS})
    target_link_libraries(kcm_kwin4_genericscripted
        KF${KF_MAJOR_VERSION}::ConfigWidgets #KCModule
        KF${KF_MAJOR_VERSION}::I18n
        KF${KF_MAJOR_VERSION}::Service
        Qt${QT_MAJOR_VERSION}::DBus
        Qt${QT_MAJOR_VERSION}::UiTools
        kwineffects
    )
    install(TARGETS kcm_kwin4_genericscripted DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin/effects/configs)
endif()

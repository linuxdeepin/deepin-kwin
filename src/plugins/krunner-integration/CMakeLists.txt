set(krunnerintegration_SOURCES
    main.cpp
    windowsrunnerinterface.cpp
)

if (QT_MAJOR_VERSION EQUAL "5")
    qt5_add_dbus_adaptor(krunnerintegration_SOURCES org.kde.krunner1.xml windowsrunnerinterface.h KWin::WindowsRunner)
else()
    qt_add_dbus_adaptor(krunnerintegration_SOURCES org.kde.krunner1.xml windowsrunnerinterface.h KWin::WindowsRunner)
endif()

add_library(krunnerintegration MODULE ${krunnerintegration_SOURCES})

set_target_properties(krunnerintegration PROPERTIES
    LIBRARY_OUTPUT_NAME "kwin5_plugin_krunner"
)

target_link_libraries(krunnerintegration kwin KF${KF_MAJOR_VERSION}::Runner)

install(TARGETS krunnerintegration DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin/plugins/)
install(FILES kwin-runner-windows.desktop DESTINATION ${KDE_INSTALL_DATAROOTDIR}/krunner/dbusplugins)

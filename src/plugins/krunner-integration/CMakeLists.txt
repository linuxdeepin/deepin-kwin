set(krunnerintegration_SOURCES
    main.cpp
    windowsrunnerinterface.cpp
)

qt_add_dbus_adaptor(krunnerintegration_SOURCES org.kde.krunner1.xml windowsrunnerinterface.h KWin::WindowsRunner)

add_library(krunnerintegration MODULE ${krunnerintegration_SOURCES})
target_link_libraries(krunnerintegration deepin-kwin KF5::Runner)

install(TARGETS krunnerintegration DESTINATION ${KDE_INSTALL_PLUGINDIR}/deepin-kwin/plugins/)
install(FILES deepin-kwin-runner-windows.desktop DESTINATION ${KDE_INSTALL_DATAROOTDIR}/krunner/dbusplugins)

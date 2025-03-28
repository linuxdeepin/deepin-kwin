# Install the KWin/WindowSwitcher service type
if (QT_MAJOR_VERSION EQUAL "5")
    install(FILES kwinwindowswitcher.desktop DESTINATION ${KDE_INSTALL_KSERVICETYPESDIR})
    install(FILES kwindesktopswitcher.desktop DESTINATION ${KDE_INSTALL_KSERVICETYPESDIR})
endif()

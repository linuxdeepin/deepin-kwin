install(
    FILES
        deepin-kglobalshortcutsrc
        deepin-kwinrc
        deepin-kwinrulesrc
        deepin-klaunchrc
        deepin-kdeglobals
    DESTINATION
        ${CMAKE_INSTALL_SYSCONFDIR}
)

install(
    FILES
        org.deepin.kwin.splitmenu.display.json
    DESTINATION
        "${CMAKE_INSTALL_DATAROOTDIR}/dsg/configs/org.deepin.kwin"
)

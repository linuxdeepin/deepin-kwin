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
        "share/dsg/configs/org.deepin.kwin"
)

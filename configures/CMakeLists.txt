install(
    FILES
        kglobalshortcutsrc
        deepin-kwinrc
        deepin-kwinrulesrc
        deepin-klaunchrc
        deepin-kdeglobals
    DESTINATION
        ${KDE_INSTALL_CONFDIR}
)

install(
    FILES
        kglobalshortcutsrc
    DESTINATION
        ${CMAKE_INSTALL_SYSCONFDIR}/skel/.config/
)

install(
    FILES
        org.deepin.kwin.splitmenu.display.json
    DESTINATION
        "share/dsg/configs/org.deepin.kwin"
)

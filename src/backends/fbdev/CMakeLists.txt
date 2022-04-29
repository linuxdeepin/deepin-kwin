set(FBDEV_SOURCES
    fb_backend.cpp
    logging.cpp
    scene_qpainter_fb_backend.cpp
)

add_library(KWinWaylandFbdevBackend MODULE ${FBDEV_SOURCES})
set_target_properties(KWinWaylandFbdevBackend PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/org.kde.deepin-kwin.waylandbackends/")
target_link_libraries(KWinWaylandFbdevBackend deepin-kwin)

install(
    TARGETS
        KWinWaylandFbdevBackend
    DESTINATION
        ${KDE_INSTALL_PLUGINDIR}/org.kde.deepin-kwin.waylandbackends/
)

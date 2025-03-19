target_sources(kwin PRIVATE
    wayland_backend.cpp
    wayland_display.cpp
    wayland_egl_backend.cpp
    wayland_logging.cpp
    wayland_output.cpp
    wayland_qpainter_backend.cpp
)

target_link_libraries(kwin 
    Wayland::Client
    gbm::gbm
)

if (QT_MAJOR_VERSION EQUAL "5")
    target_link_libraries(kwin KF5::WaylandClient)
else()
    target_link_libraries(kwin Plasma::KWaylandClient)
endif()

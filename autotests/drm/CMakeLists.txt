set(mockDRM_SRCS
    mock_drm.cpp
    ../../src/backends/drm/drm_abstract_output.cpp
    ../../src/backends/drm/drm_adaptor.cpp
    ../../src/backends/drm/drm_backend.cpp
    ../../src/backends/drm/drm_buffer.cpp
    ../../src/backends/drm/drm_buffer_gbm.cpp
    ../../src/backends/drm/drm_dmabuf_feedback.cpp
    ../../src/backends/drm/drm_dumb_buffer.cpp
    ../../src/backends/drm/drm_dumb_swapchain.cpp
    ../../src/backends/drm/drm_egl_backend.cpp
    ../../src/backends/drm/drm_egl_cursor_layer.cpp
    ../../src/backends/drm/drm_egl_layer.cpp
    ../../src/backends/drm/drm_egl_layer_surface.cpp
    ../../src/backends/drm/drm_gbm_surface.cpp
    ../../src/backends/drm/drm_gpu.cpp
    ../../src/backends/drm/drm_layer.cpp
    ../../src/backends/drm/drm_logging.cpp
    ../../src/backends/drm/drm_connector.cpp
    ../../src/backends/drm/drm_object.cpp
    ../../src/backends/drm/drm_crtc.cpp
    ../../src/backends/drm/drm_plane.cpp
    ../../src/backends/drm/drm_output.cpp
    ../../src/backends/drm/drm_pipeline.cpp
    ../../src/backends/drm/drm_pipeline_legacy.cpp
    ../../src/backends/drm/drm_property.cpp
    ../../src/backends/drm/drm_qpainter_backend.cpp
    ../../src/backends/drm/drm_qpainter_layer.cpp
    ../../src/backends/drm/drm_shadow_buffer.cpp
    ../../src/backends/drm/drm_virtual_egl_layer.cpp
    ../../src/backends/drm/drm_virtual_output.cpp
    ../../src/backends/drm/remoteaccess_manager.cpp
)

include_directories(${Libdrm_INCLUDE_DIRS})

add_library(LibDrmTest STATIC ${mockDRM_SRCS})
target_link_libraries(LibDrmTest
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Widgets
    KF${KF_MAJOR_VERSION}::ConfigCore
    KF${KF_MAJOR_VERSION}::WindowSystem
    KF${KF_MAJOR_VERSION}::CoreAddons
    KF${KF_MAJOR_VERSION}::I18n
    XCB::XCB
    PkgConfig::Libxcvt
    gbm::gbm
    kwin
)
target_include_directories(LibDrmTest
    PUBLIC
        ../../src
        ../../src/platformsupport/scenes/opengl
        ../../src/platformsupport/scenes/qpainter
        ../../src/backends/drm/
)

########################################################
# Tests
########################################################
add_executable(testDrm drmTest.cpp)
target_link_libraries(testDrm LibDrmTest Qt${QT_MAJOR_VERSION}::Test)
add_test(NAME kwin-testDrm COMMAND testDrm)
ecm_mark_as_test(testDrm)

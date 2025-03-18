add_library(KWinScreencastPlugin OBJECT)
target_sources(KWinScreencastPlugin PRIVATE
    eglnativefence.cpp
    main.cpp
    outputscreencastsource.cpp
    pipewirecore.cpp
    regionscreencastsource.cpp
    screencastmanager.cpp
    screencastsource.cpp
    screencaststream.cpp
    windowscreencastsource.cpp
)

set_target_properties(KWinScreencastPlugin PROPERTIES
    LIBRARY_OUTPUT_NAME "kwin5_plugin_screencast"
)

ecm_qt_declare_logging_category(KWinScreencastPlugin
    HEADER kwinscreencast_logging.h
    IDENTIFIER KWIN_SCREENCAST
    CATEGORY_NAME kwin_screencast
    DEFAULT_SEVERITY Warning
)

target_compile_definitions(KWinScreencastPlugin PRIVATE QT_STATICPLUGIN)
target_link_libraries(KWinScreencastPlugin kwin PkgConfig::PipeWire)

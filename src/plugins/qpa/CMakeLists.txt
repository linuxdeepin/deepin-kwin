add_library(KWinQpaPlugin OBJECT)
set_property(TARGET KWinQpaPlugin PROPERTY POSITION_INDEPENDENT_CODE ON)
target_sources(KWinQpaPlugin PRIVATE
    backingstore.cpp
    eglhelpers.cpp
    eglplatformcontext.cpp
    integration.cpp
    main.cpp
    offscreensurface.cpp
    platformcursor.cpp
    screen.cpp
    window.cpp
)

ecm_qt_declare_logging_category(KWinQpaPlugin
    HEADER logging.h
    IDENTIFIER KWIN_QPA
    CATEGORY_NAME kwin_qpa_plugin
    DEFAULT_SEVERITY Critical
)

target_compile_definitions(KWinQpaPlugin PRIVATE QT_STATICPLUGIN)

target_link_libraries(KWinQpaPlugin
    Qt${QT_MAJOR_VERSION}::CorePrivate
    Qt${QT_MAJOR_VERSION}::GuiPrivate
    Freetype::Freetype # Must be after Qt5 platform support libs
    Fontconfig::Fontconfig
    kwin
)
if (QT_MAJOR_VERSION EQUAL "5")
    target_link_libraries(KWinQpaPlugin
        Qt5FontDatabaseSupport::Qt5FontDatabaseSupport
        Qt5ThemeSupport::Qt5ThemeSupport
        Qt5EventDispatcherSupport::Qt5EventDispatcherSupport
        Qt5ServiceSupport::Qt5ServiceSupport
    )
endif()

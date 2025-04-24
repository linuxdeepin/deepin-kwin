#######################################
# Effect

# Source files
set(screenshot_SOURCES
    main.cpp
    screenshot.cpp
    screenshotdbusinterface1.cpp
    screenshotdbusinterface2.cpp
)

if (QT_MAJOR_VERSION EQUAL "5")
    qt5_add_dbus_adaptor(screenshot_SOURCES org.kde.KWin.ScreenShot2.xml screenshotdbusinterface2.h KWin::ScreenShotDBusInterface2)
else()
    qt_add_dbus_adaptor(screenshot_SOURCES org.kde.KWin.ScreenShot2.xml screenshotdbusinterface2.h KWin::ScreenShotDBusInterface2)
endif()

kwin4_add_effect_module(screenshot ${screenshot_SOURCES})
target_link_libraries(screenshot PRIVATE
    kwineffects
    kwinglutils

    KF${KF_MAJOR_VERSION}::Service
    KF${KF_MAJOR_VERSION}::I18n

    Qt${QT_MAJOR_VERSION}::Concurrent
    Qt${QT_MAJOR_VERSION}::DBus
    Qt${QT_MAJOR_VERSION}::QuickWidgets
)

if (QT_MAJOR_VERSION EQUAL "5")
    target_link_libraries(screenshot PRIVATE Qt5::X11Extras)
else()
    target_link_libraries(screenshot PRIVATE Qt6::GuiPrivate)
endif()

if (KWIN_BUILD_NOTIFICATIONS)
    target_link_libraries(screenshot PRIVATE KF${KF_MAJOR_VERSION}::Notifications)
endif()

ecm_qt_declare_logging_category(screenshot
    HEADER screenshotlogging.h
    IDENTIFIER KWIN_SCREENSHOT
    CATEGORY_NAME kwin_effect_screenshot
    DEFAULT_SEVERITY Warning
)

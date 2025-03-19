set(kwindowsystem_plugin_SRCS
    plugin.cpp
    windoweffects.cpp
    windowshadow.cpp
    windowsystem.cpp
)

add_library(KF${KF_MAJOR_VERSION}WindowSystemKWinPlugin OBJECT ${kwindowsystem_plugin_SRCS})
set_property(TARGET KF${KF_MAJOR_VERSION}WindowSystemKWinPlugin PROPERTY POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(KF${KF_MAJOR_VERSION}WindowSystemKWinPlugin PRIVATE QT_STATICPLUGIN)
target_link_libraries(KF${KF_MAJOR_VERSION}WindowSystemKWinPlugin kwin)

set(idletime_plugin_SRCS
    poller.cpp
)

add_library(KF${KF_MAJOR_VERSION}IdleTimeKWinPlugin OBJECT ${idletime_plugin_SRCS})
set_property(TARGET KF${KF_MAJOR_VERSION}IdleTimeKWinPlugin PROPERTY POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(KF${KF_MAJOR_VERSION}IdleTimeKWinPlugin PRIVATE QT_STATICPLUGIN)
target_link_libraries(KF${KF_MAJOR_VERSION}IdleTimeKWinPlugin
    KF${KF_MAJOR_VERSION}::IdleTime
    kwin
)

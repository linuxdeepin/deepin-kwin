set(kglobalaccel_plugin_SRCS
    kglobalaccel_plugin.cpp
)

find_package(KF${KF_MAJOR_VERSION} REQUIRED COMPONENTS GlobalAccel)

add_library(KF${KF_MAJOR_VERSION}GlobalAccelKWinPlugin OBJECT ${kglobalaccel_plugin_SRCS})
set_property(TARGET KF${KF_MAJOR_VERSION}GlobalAccelKWinPlugin PROPERTY POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(KF${KF_MAJOR_VERSION}GlobalAccelKWinPlugin PRIVATE QT_STATICPLUGIN)
target_link_libraries(KF${KF_MAJOR_VERSION}GlobalAccelKWinPlugin KF${KF_MAJOR_VERSION}::GlobalAccel kwin)

set(kglobalaccel_plugin_SRCS
    kglobalaccel_plugin.cpp
)

find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS GlobalAccel)
find_package(KGlobalAccelD REQUIRED)

add_library(KF6GlobalAccelKWinPlugin OBJECT ${kglobalaccel_plugin_SRCS})
set_property(TARGET KF6GlobalAccelKWinPlugin PROPERTY POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(KF6GlobalAccelKWinPlugin PRIVATE QT_STATICPLUGIN)
target_link_libraries(KF6GlobalAccelKWinPlugin KF6::GlobalAccel K::KGlobalAccelD kwin)

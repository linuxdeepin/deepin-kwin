target_sources(deepin-kwin PRIVATE
    softwarevsyncmonitor.cpp
    vsyncmonitor.cpp
)

target_include_directories(deepin-kwin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

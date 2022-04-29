target_sources(deepin-kwin PRIVATE
    qpaintersurfacetexture.cpp
    qpaintersurfacetexture_internal.cpp
    qpaintersurfacetexture_wayland.cpp
    qpainterbackend.cpp
)
target_include_directories(deepin-kwin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

#######################################
# Effect

# Source files
set(alttabthumbnaillist_SOURCES
    alttabthumbnaillist.cpp
    alttabthumbnaillist.qrc
    main.cpp
)
kwin4_add_effect_module(alttabthumbnaillist ${alttabthumbnaillist_SOURCES})
target_link_libraries(alttabthumbnaillist PRIVATE
    kwineffects
    kwinglutils
    Qt${QT_MAJOR_VERSION}::QuickWidgets
)

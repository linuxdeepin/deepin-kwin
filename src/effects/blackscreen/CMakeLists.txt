set(blackscreen_SOURCES
    main.cpp
    blackscreen.cpp
)

kwin4_add_effect_module(blackscreen ${blackscreen_SOURCES})
target_link_libraries(blackscreen PRIVATE
    kwineffects
    kwinglutils

    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::DBus
)

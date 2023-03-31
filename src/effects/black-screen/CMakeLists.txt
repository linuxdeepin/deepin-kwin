set(blackscreen_SOURCES
    black-screen.h
    black-screen.cpp
    main.cpp
)

kwin4_add_effect_module(kwin4_effect_blackscreen ${blackscreen_SOURCES})

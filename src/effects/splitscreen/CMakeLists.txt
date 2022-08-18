#######################################
# Effect

# Source files
set(splitscreen_SOURCES
    splitscreen.cpp
    splitscreen.qrc
    main.cpp
)
kwin4_add_effect_module(kwin4_effect_splitscreen ${splitscreen_SOURCES})
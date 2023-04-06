#######################################
# Effect

set(scissorwindow_SOURCES
    main.cpp
    scissorwindow.cpp
    scissor.qrc
)

kwin4_add_effect_module(kwin4_effect_scissorwindow ${scissorwindow_SOURCES})

#######################################
# Effect

set(scissorwindow_SOURCES
    main.cpp
    scissorwindow.cpp
    scissor.qrc
)

kwin4_add_effect_module(scissor ${scissorwindow_SOURCES})
target_link_libraries(scissor PRIVATE
    kwineffects
    kwinglutils

    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::QuickWidgets
)

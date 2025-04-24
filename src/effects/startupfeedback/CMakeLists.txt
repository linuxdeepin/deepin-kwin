#######################################
# Effect

# Source files
set(startupfeedback_SOURCES
    main.cpp
    startupfeedback.cpp
    startupfeedback.qrc
)

kwin4_add_effect_module(startupfeedback ${startupfeedback_SOURCES})
target_link_libraries(startupfeedback PRIVATE
    kwineffects
    kwinglutils

    Qt${QT_MAJOR_VERSION}::DBus
    Qt${QT_MAJOR_VERSION}::Widgets
)

#######################################
# Effect

set(meetingprivacy_SOURCES
    main.cpp
    meetingprivacy.cpp
)

kwin4_add_effect_module(meetingprivacy ${meetingprivacy_SOURCES})
target_link_libraries(meetingprivacy PRIVATE
    kwineffects
    kwinglutils

    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Svg
)

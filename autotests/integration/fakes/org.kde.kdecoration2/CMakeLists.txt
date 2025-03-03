########################################################
# FakeDecoWithShadows
########################################################
add_library(fakedecoshadows MODULE fakedecoration_with_shadows.cpp)
set_target_properties(fakedecoshadows PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/fakes/org.kde.KDecoration3")
target_link_libraries(fakedecoshadows
    PUBLIC
        Qt${QT_MAJOR_VERSION}::Core
        Qt${QT_MAJOR_VERSION}::Gui
    PRIVATE
        KDecoration3::KDecoration
        KF6::CoreAddons)


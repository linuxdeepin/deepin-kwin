set(TARGET_NAME "deepin_chameleon")

add_definitions(-DTARGET_NAME=\\"${TARGET_NAME}\\")

find_package(Qt${QT_MAJOR_VERSION} CONFIG REQUIRED COMPONENTS Core)
find_package(KF${KF_MAJOR_VERSION}CoreAddons REQUIRED)
find_package(Qt${QT_MAJOR_VERSION}DBus REQUIRED)
find_package(Qt${QT_MAJOR_VERSION}Qml REQUIRED)
find_package(X11 REQUIRED)
find_package(KF${KF_MAJOR_VERSION}Config REQUIRED)
find_package(KF${KF_MAJOR_VERSION}WindowSystem REQUIRED)
find_package(Qt${QT_MAJOR_VERSION}LinguistTools REQUIRED)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed
set(CMAKE_AUTOMOC ON)
# Create code from a list of Qt designer ui files
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(
    ${TARGET_NAME}
    MODULE
    main.cpp
    chameleon.cpp
    chameleontheme.cpp
    chameleonbutton.cpp
    chameleonshadow.cpp
    chameleonconfig.cpp
    chameleonwindowtheme.cpp
    kwinutils.cpp
    themes.qrc
)
set_target_properties(${TARGET_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/org.kde.kdecoration2/"
    LIBRARY_OUTPUT_NAME "com.deepin.chameleon"
)

target_link_libraries(
    ${TARGET_NAME}
    PUBLIC
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::DBus
    Qt${QT_MAJOR_VERSION}::Qml
    X11
    XCB::XCB
    kwineffects
    KDecoration2::KDecoration
    KF${KF_MAJOR_VERSION}::CoreAddons
    KF${KF_MAJOR_VERSION}::ConfigCore
    KF${KF_MAJOR_VERSION}::WindowSystem
    kwin
)

if (QT_MAJOR_VERSION EQUAL "5")
    find_package(Qt5X11Extras REQUIRED)
    include_directories(${Qt5Core_PRIVATE_INCLUDE_DIRS})
    include_directories(${Qt5Gui_PRIVATE_INCLUDE_DIRS})
    target_link_libraries(
        ${TARGET_NAME}
        PUBLIC
        Qt5::X11Extras
    )
endif()

install(TARGETS ${TARGET_NAME} DESTINATION ${KDE_INSTALL_PLUGINDIR}/org.kde.kdecoration2)

set(TARGET_NAME "deepin_chameleon")

add_definitions(-DTARGET_NAME=\\"${TARGET_NAME}\\")

find_package(Qt6 REQUIRED COMPONENTS Core DBus LinguistTools)

#find_package(Qt5X11Extras REQUIRED)
find_package(KF6CoreAddons REQUIRED)
find_package(X11 REQUIRED)
find_package(KF6Config REQUIRED)
find_package(KF6WindowSystem REQUIRED)
find_package(KDecoration3 REQUIRED)
#include_directories(${Qt5Core_PRIVATE_INCLUDE_DIRS})
#include_directories(${Qt5Gui_PRIVATE_INCLUDE_DIRS})

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
set_target_properties(${TARGET_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/org.kde.KDecoration3/")

target_link_libraries(
    ${TARGET_NAME}
    PUBLIC
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::DBus
    #Qt${QT_MAJOR_VERSION}::X11Extras
    #Qt${QT_MAJOR_VERSION}::Quick
    Qt${QT_MAJOR_VERSION}::GuiPrivate
    X11
    XCB::XCB
    kwineffects
    KDecoration3::KDecoration
    KF6::CoreAddons
    KF6::ConfigCore
    KF6::WindowSystem
    kwin
)

install(TARGETS ${TARGET_NAME} DESTINATION ${KDE_INSTALL_PLUGINDIR}/org.kde.KDecoration3)

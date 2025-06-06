include_directories(${KWin_SOURCE_DIR}/src)
add_definitions(-DKWIN_UNIT_TEST)
########################################################
# Test TabBox::ClientModel
########################################################
set(testTabBoxClientModel_SRCS
    ../../src/tabbox/clientmodel.cpp
    ../../src/tabbox/desktopmodel.cpp
    ../../src/tabbox/tabbox_logging.cpp
    ../../src/tabbox/tabboxconfig.cpp
    ../../src/tabbox/tabboxhandler.cpp
    mock_tabboxclient.cpp
    mock_tabboxhandler.cpp
    test_tabbox_clientmodel.cpp
)

add_executable(testTabBoxClientModel ${testTabBoxClientModel_SRCS})
set_target_properties(testTabBoxClientModel PROPERTIES COMPILE_DEFINITIONS "NO_NONE_WINDOW")
target_link_libraries(testTabBoxClientModel
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::DBus
    Qt${QT_MAJOR_VERSION}::Quick
    Qt${QT_MAJOR_VERSION}::Test
    Qt${QT_MAJOR_VERSION}::Widgets
    Qt${QT_MAJOR_VERSION}::GuiPrivate

    KF${KF_MAJOR_VERSION}::ConfigCore
    KF${KF_MAJOR_VERSION}::I18n
    KF${KF_MAJOR_VERSION}::Package
    KF${KF_MAJOR_VERSION}::WindowSystem

    XCB::XCB
)
if (QT_MAJOR_VERSION EQUAL "5")
    target_link_libraries(testTabBoxClientModel Qt${QT_MAJOR_VERSION}::X11Extras)
endif()
add_test(NAME kwin-testTabBoxClientModel COMMAND testTabBoxClientModel)
ecm_mark_as_test(testTabBoxClientModel)

########################################################
# Test TabBox::TabBoxHandler
########################################################
set(testTabBoxHandler_SRCS
    ../../src/tabbox/clientmodel.cpp
    ../../src/tabbox/desktopmodel.cpp
    ../../src/tabbox/tabbox_logging.cpp
    ../../src/tabbox/tabboxconfig.cpp
    ../../src/tabbox/tabboxhandler.cpp
    mock_tabboxclient.cpp
    mock_tabboxhandler.cpp
    test_tabbox_handler.cpp
)

add_executable(testTabBoxHandler ${testTabBoxHandler_SRCS})
set_target_properties(testTabBoxHandler PROPERTIES COMPILE_DEFINITIONS "NO_NONE_WINDOW")
target_link_libraries(testTabBoxHandler
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::DBus
    Qt${QT_MAJOR_VERSION}::Quick
    Qt${QT_MAJOR_VERSION}::Test
    Qt${QT_MAJOR_VERSION}::Widgets
    Qt${QT_MAJOR_VERSION}::GuiPrivate

    KF${KF_MAJOR_VERSION}::ConfigCore
    KF${KF_MAJOR_VERSION}::I18n
    KF${KF_MAJOR_VERSION}::Package
    KF${KF_MAJOR_VERSION}::WindowSystem

    XCB::XCB
)
if (QT_MAJOR_VERSION EQUAL "5")
    target_link_libraries(testTabBoxHandler Qt${QT_MAJOR_VERSION}::X11Extras)
endif()
add_test(NAME kwin-testTabBoxHandler COMMAND testTabBoxHandler)
ecm_mark_as_test(testTabBoxHandler)

########################################################
# Test TabBox::TabBoxConfig
########################################################
set(testTabBoxConfig_SRCS
    ../../src/tabbox/tabbox_logging.cpp
    ../../src/tabbox/tabboxconfig.cpp
    test_tabbox_config.cpp
)

add_executable(testTabBoxConfig ${testTabBoxConfig_SRCS})
target_link_libraries(testTabBoxConfig Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Test)
add_test(NAME kwin-testTabBoxConfig COMMAND testTabBoxConfig)
ecm_mark_as_test(testTabBoxConfig)


########################################################
# Test TabBox::DesktopChainManager
########################################################
set(testDesktopChain_SRCS
    ../../src/tabbox/desktopchain.cpp
    ../../src/tabbox/tabbox_logging.cpp
    test_desktopchain.cpp
)

add_executable(testDesktopChain ${testDesktopChain_SRCS})
target_link_libraries(testDesktopChain Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Test)
add_test(NAME kwin-testDesktopChain COMMAND testDesktopChain)
ecm_mark_as_test(testDesktopChain)

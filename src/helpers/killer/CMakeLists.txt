########### next target ###############

set(kwin_killer_helper_SRCS killer.cpp)

add_executable(kwin_killer_helper ${kwin_killer_helper_SRCS})

target_link_libraries(kwin_killer_helper
    KF${KF_MAJOR_VERSION}::AuthCore
    KF${KF_MAJOR_VERSION}::I18n
    KF${KF_MAJOR_VERSION}::WidgetsAddons
    Qt${QT_MAJOR_VERSION}::Widgets
)
if (QT_MAJOR_VERSION EQUAL "5")
    target_link_libraries(kwin_killer_helper Qt${QT_MAJOR_VERSION}::X11Extras)
else()
    target_link_libraries(kwin_killer_helper Qt${QT_MAJOR_VERSION}::GuiPrivate)
endif()

install(TARGETS kwin_killer_helper DESTINATION ${KDE_INSTALL_LIBEXECDIR})

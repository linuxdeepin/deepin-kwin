add_subdirectory(icons)

########### next target ###############
add_executable(deepin-kwin5_update_default_rules update_default_rules.cpp)
target_link_libraries(deepin-kwin5_update_default_rules
    KF5::ConfigCore
    Qt::Core
    Qt::DBus
)
install(TARGETS deepin-kwin5_update_default_rules DESTINATION ${KDE_INSTALL_LIBDIR}/kconf_update_bin/)

########### install files ###############

install(FILES org_kde_deepin-kwin.categories DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR})

add_subdirectory(icons)

########### next target ###############
add_executable(kwin5_update_default_rules update_default_rules.cpp)
target_link_libraries(kwin5_update_default_rules
    KF${KF_MAJOR_VERSION}::ConfigCore
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::DBus
)
install(TARGETS kwin5_update_default_rules DESTINATION ${KDE_INSTALL_LIBDIR}/kconf_update_bin/)

########### install files ###############

install(FILES org_kde_kwin.categories DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR})

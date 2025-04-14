add_definitions(-DTRANSLATION_DOMAIN=\"kwin_package_effect\")

if (KF_MAJOR_VERSION EQUAL "5")
   kcoreaddons_add_plugin(kwin_packagestructure_effect SOURCES effect.cpp INSTALL_NAMESPACE "kpackage/packagestructure")
else()
   kcoreaddons_add_plugin(kwin_packagestructure_effect SOURCES effect.cpp INSTALL_NAMESPACE "kf6/packagestructure")
endif()
target_link_libraries(kwin_packagestructure_effect
   KF${KF_MAJOR_VERSION}::I18n
   KF${KF_MAJOR_VERSION}::Package
)
set_target_properties(kwin_packagestructure_effect PROPERTIES OUTPUT_NAME kwin_effect)

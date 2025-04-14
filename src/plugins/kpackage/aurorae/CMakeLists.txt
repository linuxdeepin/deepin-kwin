add_definitions(-DTRANSLATION_DOMAIN=\"kwin_package_aurorae\")

if (KF_MAJOR_VERSION EQUAL "5")
   kcoreaddons_add_plugin(kwin_packagestructure_aurorae SOURCES aurorae.cpp INSTALL_NAMESPACE "kpackage/packagestructure")
else()
   kcoreaddons_add_plugin(kwin_packagestructure_aurorae SOURCES aurorae.cpp INSTALL_NAMESPACE "kf6/packagestructure")
endif()

target_link_libraries(kwin_packagestructure_aurorae
   KF${KF_MAJOR_VERSION}::I18n
   KF${KF_MAJOR_VERSION}::Package
)
set_target_properties(kwin_packagestructure_aurorae PROPERTIES OUTPUT_NAME kwin_aurorae)

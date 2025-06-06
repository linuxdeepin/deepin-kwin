add_library(colordintegration MODULE)

set_target_properties(colordintegration PROPERTIES
    LIBRARY_OUTPUT_NAME "kwin5_plugin_colord"
)

target_sources(colordintegration PRIVATE
    colorddevice.cpp
    colordintegration.cpp
    main.cpp
)

ecm_qt_declare_logging_category(colordintegration
    HEADER colordlogging.h
    IDENTIFIER KWIN_COLORD
    CATEGORY_NAME kwin_colord
    DEFAULT_SEVERITY Warning
    DESCRIPTION "KWin colord integration"
)

set(colordintegration_xml_SOURCES)
set(COLORD_DEVICE_XML org.freedesktop.ColorManager.Device.xml)
set(COLORD_PROFILE_XML org.freedesktop.ColorManager.Profile.xml)
set(COLORD_MANAGER_XML org.freedesktop.ColorManager.xml)

set_source_files_properties(${COLORD_MANAGER_XML} PROPERTIES INCLUDE "colordtypes.h")
set_source_files_properties(${COLORD_MANAGER_XML} PROPERTIES NO_NAMESPACE true)
set_source_files_properties(${COLORD_MANAGER_XML} PROPERTIES CLASSNAME CdInterface)
if (QT_MAJOR_VERSION EQUAL "5")
    qt5_add_dbus_interface(colordintegration_xml_SOURCES ${COLORD_MANAGER_XML} colordinterface)
else()
    qt_add_dbus_interface(colordintegration_xml_SOURCES ${COLORD_MANAGER_XML} colordinterface)
endif()

set_source_files_properties(${COLORD_DEVICE_XML} PROPERTIES INCLUDE "colordtypes.h")
set_source_files_properties(${COLORD_DEVICE_XML} PROPERTIES NO_NAMESPACE true)
set_source_files_properties(${COLORD_DEVICE_XML} PROPERTIES CLASSNAME CdDeviceInterface)
if (QT_MAJOR_VERSION EQUAL "5")
    qt5_add_dbus_interface(colordintegration_xml_SOURCES ${COLORD_DEVICE_XML} colorddeviceinterface)
else()
    qt_add_dbus_interface(colordintegration_xml_SOURCES ${COLORD_DEVICE_XML} colorddeviceinterface)
endif()

set_source_files_properties(${COLORD_PROFILE_XML} PROPERTIES INCLUDE "colordtypes.h")
set_source_files_properties(${COLORD_PROFILE_XML} PROPERTIES NO_NAMESPACE true)
set_source_files_properties(${COLORD_PROFILE_XML} PROPERTIES CLASSNAME CdProfileInterface)
if (QT_MAJOR_VERSION EQUAL "5")
    qt5_add_dbus_interface(colordintegration_xml_SOURCES ${COLORD_PROFILE_XML} colordprofileinterface)
else()
    qt_add_dbus_interface(colordintegration_xml_SOURCES ${COLORD_PROFILE_XML} colordprofileinterface)
endif()

target_sources(colordintegration PRIVATE ${colordintegration_xml_SOURCES})
set_target_properties(colordintegration PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/kwin/plugins/")
target_link_libraries(colordintegration kwin)
install(TARGETS colordintegration DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin/plugins/)

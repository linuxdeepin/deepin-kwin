########### next target ###############
include(ECMSetupVersion)
include(ECMSetupQtPluginMacroNames)

ecm_setup_version(${PROJECT_VERSION}
                  VARIABLE_PREFIX DEEPIN_KWINEFFECTS
                  VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/deepin-kwineffects_version.h"
                  PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/DeepinKWinEffectsConfigVersion.cmake"
                  SOVERSION 14
)

ecm_setup_qtplugin_macro_names(
    JSON_ARG2
        "KWIN_EFFECT_FACTORY"
    JSON_ARG3
        "KWIN_EFFECT_FACTORY_ENABLED"
        "KWIN_EFFECT_FACTORY_SUPPORTED"
    JSON_ARG4
        "KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED"
    CONFIG_CODE_VARIABLE
        PACKAGE_SETUP_AUTOMOC_VARIABLES
)

###  effects lib  ###
set(kwin_EFFECTSLIB_SRCS
    anidata.cpp
    deepin-kwinanimationeffect.cpp
    deepin-kwineffects.cpp
    deepin-kwineffectsex.cpp
    deepin-kwinoffscreeneffect.cpp
    deepin-kwinoffscreenquickview.cpp
    deepin-kwinquickeffect.cpp
    logging.cpp
    sharedqmlengine.cpp
)

add_library(deepin-kwineffects SHARED ${kwin_EFFECTSLIB_SRCS})
generate_export_header(deepin-kwineffects EXPORT_FILE_NAME deepin-kwineffects_export.h)
target_link_libraries(deepin-kwineffects
    PUBLIC
        KF5::ConfigCore
        KF5::CoreAddons
        KF5::WindowSystem
        XCB::XCB
    PRIVATE
        Qt${QT_MAJOR_VERSION}::Quick
        KF5::I18n
        deepin-kwinglutils
)
if (QT_MAJOR_VERSION EQUAL "6")
    target_link_libraries(deepin-kwineffects PRIVATE Qt${QT_MAJOR_VERSION}::GuiPrivate)
endif()
if (KWIN_HAVE_XRENDER_COMPOSITING)
    target_link_libraries(deepin-kwineffects PRIVATE XCB::XFIXES)
endif()
set_target_properties(deepin-kwineffects PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

install(TARGETS deepin-kwineffects EXPORT DeepinKWinEffectsTargets ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

# kwingl(es)utils library
set(kwin_GLUTILSLIB_SRCS
    deepin-kwinglplatform.cpp
    deepin-kwingltexture.cpp
    deepin-kwinglutils.cpp
    deepin-kwinglutils_funcs.cpp
    deepin-kwineglimagetexture.cpp
    logging.cpp
)

add_library(deepin-kwinglutils SHARED ${kwin_GLUTILSLIB_SRCS})
generate_export_header(deepin-kwinglutils BASE_NAME deepin-kwinglutils EXPORT_FILE_NAME deepin-kwinglutils_export.h)
target_link_libraries(deepin-kwinglutils PUBLIC XCB::XCB KF5::CoreAddons KF5::ConfigCore KF5::WindowSystem epoxy::epoxy)
set_target_properties(deepin-kwinglutils PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

install(TARGETS deepin-kwinglutils EXPORT DeepinKWinEffectsTargets ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/deepin-kwinconfig.h
    ${CMAKE_CURRENT_BINARY_DIR}/deepin-kwineffects_export.h
    ${CMAKE_CURRENT_BINARY_DIR}/deepin-kwinglutils_export.h
    deepin-kwinanimationeffect.h
    deepin-kwineffects.h
    deepin-kwineffectsex.h
    deepin-kwinglobals.h
    deepin-kwinglplatform.h
    deepin-kwingltexture.h
    deepin-kwinglutils.h
    deepin-kwinglutils_funcs.h
    deepin-kwinoffscreeneffect.h
    deepin-kwinoffscreenquickview.h
    deepin-kwinquickeffect.h
    DESTINATION ${KDE_INSTALL_INCLUDEDIR} COMPONENT Devel)

set(CMAKECONFIG_INSTALL_DIR "${KDE_INSTALL_CMAKEPACKAGEDIR}/DeepinKWinEffects")


install(FILES  "${CMAKE_CURRENT_BINARY_DIR}/DeepinKWinEffectsConfig.cmake"
               "${CMAKE_CURRENT_BINARY_DIR}/DeepinKWinEffectsConfigVersion.cmake"
        DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
        COMPONENT Devel )

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/DeepinKWinEffectsConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/DeepinKWinEffectsConfig.cmake"
  INSTALL_DESTINATION  ${CMAKECONFIG_INSTALL_DIR}
)

install(EXPORT DeepinKWinEffectsTargets DESTINATION "${CMAKECONFIG_INSTALL_DIR}" FILE DeepinKWinEffectsTargets.cmake NAMESPACE DeepinKWinEffects:: )

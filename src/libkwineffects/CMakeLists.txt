########### next target ###############
include(ECMSetupVersion)

ecm_setup_version(${PROJECT_VERSION}
                  VARIABLE_PREFIX KWINEFFECTS
                  VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/kwineffects_version.h"
                  PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KWinEffectsConfigVersion.cmake"
                  SOVERSION 13
)

### xrenderutils lib ###
set(kwin_XRENDERUTILS_SRCS
    kwinxrenderutils.cpp
    logging.cpp
)

add_library(deepin-kwinxrenderutils SHARED ${kwin_XRENDERUTILS_SRCS})
generate_export_header(deepin-kwinxrenderutils BASE_NAME kwinxrenderutils EXPORT_FILE_NAME deepin_kwinxrenderutils_export.h)
target_link_libraries(deepin-kwinxrenderutils
    PUBLIC
        Qt::Core
        Qt::Gui

        XCB::RENDER
        XCB::XCB
        XCB::XFIXES
)

set_target_properties(deepin-kwinxrenderutils PROPERTIES
    VERSION ${KWINEFFECTS_VERSION}
    SOVERSION ${KWINEFFECTS_SOVERSION}
)
set_target_properties(deepin-kwinxrenderutils PROPERTIES OUTPUT_NAME ${KWIN_NAME}xrenderutils)

install(TARGETS deepin-kwinxrenderutils EXPORT KWinEffectsTargets ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

###  effects lib  ###
set(kwin_EFFECTSLIB_SRCS
    anidata.cpp
    deepin_kwinanimationeffect.cpp
    deepin_kwindeformeffect.cpp
    deepin_kwineffects.cpp
    deepin_kwineffectsex.cpp
    deepin_kwinoffscreenquickview.cpp
    deepin_kwinquickeffect.cpp
    logging.cpp
)

add_library(deepin-kwineffects SHARED ${kwin_EFFECTSLIB_SRCS})
generate_export_header(deepin-kwineffects BASE_NAME kwineffects EXPORT_FILE_NAME deepin_kwineffects_export.h)
target_link_libraries(deepin-kwineffects
    PUBLIC
        KF5::ConfigCore
        KF5::CoreAddons
        KF5::WindowSystem
        XCB::XCB
    PRIVATE
        Qt::Quick
        KF5::Declarative
        deepin-kwinglutils
)
set_target_properties(deepin-kwineffects PROPERTIES
    VERSION ${KWINEFFECTS_VERSION}
    SOVERSION ${KWINEFFECTS_SOVERSION}
)
set_target_properties(deepin-kwineffects PROPERTIES OUTPUT_NAME ${KWIN_NAME}effects)

install(TARGETS deepin-kwineffects EXPORT KWinEffectsTargets ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

# kwingl(es)utils library
set(kwin_GLUTILSLIB_SRCS
    kwinglplatform.cpp
    kwingltexture.cpp
    kwinglutils.cpp
    kwinglutils_funcs.cpp
    kwineglimagetexture.cpp
    logging.cpp
)

add_library(deepin-kwinglutils SHARED ${kwin_GLUTILSLIB_SRCS})
generate_export_header(deepin-kwinglutils BASE_NAME kwinglutils EXPORT_FILE_NAME deepin_kwinglutils_export.h)
target_link_libraries(deepin-kwinglutils PUBLIC XCB::XCB KF5::CoreAddons KF5::ConfigCore KF5::WindowSystem epoxy::epoxy)
set_target_properties(deepin-kwinglutils PROPERTIES
    VERSION ${KWINEFFECTS_VERSION}
    SOVERSION ${KWINEFFECTS_SOVERSION}
    OUTPUT_NAME ${KWIN_NAME}glutils
)

install(TARGETS deepin-kwinglutils EXPORT KWinEffectsTargets ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/deepin_kwinconfig.h
    ${CMAKE_CURRENT_BINARY_DIR}/deepin_kwineffects_export.h
    ${CMAKE_CURRENT_BINARY_DIR}/deepin_kwinglutils_export.h
    ${CMAKE_CURRENT_BINARY_DIR}/deepin_kwinxrenderutils_export.h
    deepin_kwinanimationeffect.h
    deepin_kwindeformeffect.h
    deepin_kwineffects.h
    deepin_kwineffectsex.h
    deepin_kwinglobals.h
    deepin_kwinglplatform.h
    deepin_kwingltexture.h
    deepin_kwinglutils.h
    deepin_kwinglutils_funcs.h
    deepin_kwinoffscreenquickview.h
    deepin_kwinquickeffect.h
    deepin_kwinxrenderutils.h
    DESTINATION ${KDE_INSTALL_INCLUDEDIR} COMPONENT Devel)

set(CMAKECONFIG_INSTALL_DIR "${KDE_INSTALL_CMAKEPACKAGEDIR}/DeepinKWinEffects")


install(FILES  "${CMAKE_CURRENT_BINARY_DIR}/KWinEffectsConfig.cmake"
               "${CMAKE_CURRENT_BINARY_DIR}/KWinEffectsConfigVersion.cmake"
        DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
        COMPONENT Devel )

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/KWinEffectsConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/KWinEffectsConfig.cmake"
  INSTALL_DESTINATION  ${CMAKECONFIG_INSTALL_DIR}
)

install(EXPORT KWinEffectsTargets DESTINATION "${CMAKECONFIG_INSTALL_DIR}" FILE KWinEffectsTargets.cmake NAMESPACE DeepinKWinEffects:: )

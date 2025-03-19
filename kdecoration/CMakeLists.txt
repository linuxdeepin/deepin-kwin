cmake_minimum_required(VERSION 3.16)

project(decoration-api)
set(PROJECT_VERSION "5.27.5")

set(QT_MIN_VERSION "5.15.2")
set(KF5_MIN_VERSION "5.102.0")
set(KDE_COMPILERSETTINGS_LEVEL "5.82")

find_package(ECM ${KF5_MIN_VERSION} REQUIRED NO_MODULE)
# where to look first for cmake modules, before ${CMAKE_ROOT}/Modules/ is checked
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH} )

include(KDEInstallDirs)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings)
include(ECMSetupVersion)
include(ECMGenerateHeaders)
include(ECMGenerateExportHeader)
include(FeatureSummary)
include(GenerateExportHeader)
include(CMakePackageConfigHelpers)
include(KDEClangFormat)
include(KDEGitCommitHooks)

ecm_setup_version(${PROJECT_VERSION} VARIABLE_PREFIX KDECORATION2
                        VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/kdecoration2_version.h"
                        PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KDecoration2ConfigVersion.cmake"
                        SOVERSION 5)

#dependencies
find_package(Qt${QT_MAJOR_VERSION} CONFIG REQUIRED COMPONENTS
    Core
    Gui
    Test
)

find_package(KF${KF_MAJOR_VERSION} REQUIRED COMPONENTS
    CoreAddons
    I18n
)

# require at least gcc 4.8
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    if ("${CMAKE_CXX_COMPILER_VERSION}" VERSION_LESS "4.8")
        message(SEND_ERROR "Version ${CMAKE_CXX_COMPILER_VERSION} of the ${CMAKE_CXX_COMPILER_ID} C++ compiler is not supported. Please use version 4.8 or later.")
    endif()
endif()

set(KDECORATION2_INCLUDEDIR "${KDE_INSTALL_INCLUDEDIR}/KDecoration2")

add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0x050d00)
add_definitions(-DKF_DISABLE_DEPRECATED_BEFORE_AND_AT=0x054200)
add_definitions(-DQT_NO_KEYWORDS)

# Subdirectories
add_subdirectory(src)
if(BUILD_TESTING)
   add_subdirectory(autotests)
endif()

# add clang-format target for all our real source files
file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES *.cpp *.h)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})
kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)

# create a Config.cmake and a ConfigVersion.cmake file and install them
#set(CMAKECONFIG_INSTALL_DIR "${KDE_INSTALL_CMAKEPACKAGEDIR}/KDecoration2")

#configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/KDecoration2Config.cmake.in"
#                                  "${CMAKE_CURRENT_BINARY_DIR}/KDecoration2Config.cmake"
#                                  INSTALL_DESTINATION  ${CMAKECONFIG_INSTALL_DIR}
#                                  )

# install(FILES  "${CMAKE_CURRENT_BINARY_DIR}/KDecoration2Config.cmake"
#                "${CMAKE_CURRENT_BINARY_DIR}/KDecoration2ConfigVersion.cmake"
#         DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
#         COMPONENT Devel )

# install(EXPORT KDecoration2Targets DESTINATION "${CMAKECONFIG_INSTALL_DIR}" FILE KDecoration2Targets.cmake NAMESPACE KDecoration2:: )


# install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kdecoration2_version.h
#         DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF} COMPONENT Devel )

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

#ki18n_install(po)

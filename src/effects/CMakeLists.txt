# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kwin_effects\" -DEFFECT_BUILTINS)

include_directories(${KWin_SOURCE_DIR}/src) # for xcbutils.h

add_subdirectory(private)


function(kwin_add_effect_config name)
    list(REMOVE_ITEM ARGV ${name})
    kcoreaddons_add_plugin(${name} INSTALL_NAMESPACE "kwin/effects/configs" SOURCES ${ARGV})
endfunction()

# Add a CMake-time check for python3 to avoid failures during build.
find_package (Python3 COMPONENTS Interpreter)
add_feature_info("Python3" Python3_Interpreter_FOUND "Required to strip effects metadata")
set(KSEM_EXE "${CMAKE_CURRENT_SOURCE_DIR}/strip-effect-metadata.py")

function (kwin_strip_builtin_effect_metadata target metadata)
    set(stripped_metadata "${CMAKE_CURRENT_BINARY_DIR}/${metadata}.stripped")

    set(command ${KSEM_EXE} --source=${metadata} --output=${stripped_metadata})
    add_custom_command(
        OUTPUT ${stripped_metadata}
        COMMAND ${command}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS ${metadata}
        COMMENT "Stripping ${metadata}..."
    )
    set_property(TARGET ${target} APPEND PROPERTY AUTOGEN_TARGET_DEPENDS ${stripped_metadata})
endfunction()

if (QT_MAJOR_VERSION EQUAL "5")
    pkg_search_module(QGSETTINGS REQUIRED IMPORTED_TARGET gsettings-qt)
else()
    pkg_search_module(QGSETTINGS REQUIRED IMPORTED_TARGET gsettings-qt6)
endif()
find_package(Qt${QT_MAJOR_VERSION}LinguistTools REQUIRED)

macro(KWIN4_ADD_EFFECT_MODULE name)
    kcoreaddons_add_plugin(${name} STATIC SOURCES ${ARGN} INSTALL_NAMESPACE "kwin/effects/plugins")
    set_property(TARGET ${name} PROPERTY POSITION_INDEPENDENT_CODE ON)
    kwin_strip_builtin_effect_metadata(${name} metadata.json)
    install(FILES metadata.json DESTINATION ${KDE_INSTALL_DATADIR}/kwin/builtin-effects/${name}/)
endmacro()

# Install the KWin/Effect service type
if (KF_MAJOR_VERSION EQUAL "5")
    install(FILES kwineffect.desktop DESTINATION ${KDE_INSTALL_KSERVICETYPESDIR})
endif()

# scripted effects
function(install_scripted_effect name)
    kpackage_install_package(${name}/package kwin4_effect_${name} effects kwin)

    # necessary so tests are found without installing
    file(COPY ${name}/package/contents ${name}/package/metadata.json DESTINATION ${CMAKE_BINARY_DIR}/bin/kwin/effects/kwin4_effect_${name})
endfunction()
install_scripted_effect(dialogparent)
install_scripted_effect(dimscreen)
install_scripted_effect(eyeonscreen)
install_scripted_effect(fade)
install_scripted_effect(fadedesktop)
install_scripted_effect(fadingpopups)
install_scripted_effect(frozenapp)
install_scripted_effect(fullscreen)
install_scripted_effect(login)
install_scripted_effect(logout)
install_scripted_effect(maximize)
install_scripted_effect(morphingpopups)
install_scripted_effect(scale)
install_scripted_effect(squash)
install_scripted_effect(translucency)
install_scripted_effect(windowaperture)
install_scripted_effect(sessionquit)

find_package(Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
   QuickWidgets
)

###############################################################################
# Built-in effects go here

# Common effects
add_subdirectory(colorpicker)
add_subdirectory(desktopgrid)
add_subdirectory(diminactive)
add_subdirectory(fallapart)
add_subdirectory(highlightwindow)
add_subdirectory(kscreen)
add_subdirectory(screentransform)
add_subdirectory(magiclamp)
add_subdirectory(outputlocator)
add_subdirectory(overview)
add_subdirectory(screenedge)
add_subdirectory(showfps)
add_subdirectory(showpaint)
add_subdirectory(slide)
add_subdirectory(slideback)
add_subdirectory(slidingpopups)
add_subdirectory(thumbnailaside)
add_subdirectory(tileseditor)
add_subdirectory(touchpoints)
add_subdirectory(zoom)
add_subdirectory(splitscreen)
add_subdirectory(splitswap)
add_subdirectory(maximizeex)

# OpenGL-specific effects
add_subdirectory(blendchanges)
add_subdirectory(blur)
add_subdirectory(backgroundcontrast)
add_subdirectory(glide)
add_subdirectory(invert)
add_subdirectory(magnifier)
add_subdirectory(mouseclick)
add_subdirectory(mousemark)
add_subdirectory(screenshot)
add_subdirectory(sheet)
add_subdirectory(snaphelper)
add_subdirectory(startupfeedback)
add_subdirectory(trackmouse)
add_subdirectory(wobblywindows)
add_subdirectory(scissorwindow)
add_subdirectory(multitaskview)
add_subdirectory(alttabthumbnaillist)

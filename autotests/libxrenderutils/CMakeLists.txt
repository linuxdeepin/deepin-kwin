#message(STATUS "TTTTTTTTTTTTTT : ${CMAKE_SAFETYTEST}")
#message(STATUS "VVVVVVVVVVVVVV : ${CMAKE_CXX_FLAGS}")
include_directories(../)
add_executable(blendPictureTest blendpicture_test.cpp  ../testprintasanbase.cpp)
set_target_properties(blendPictureTest PROPERTIES COMPILE_DEFINITIONS "NO_NONE_WINDOW")
add_test(NAME xrenderutils-blendPictureTest COMMAND blendPictureTest)
target_link_libraries(blendPictureTest
    kwinxrenderutils
    Qt5::Test
    Qt5::Gui
    Qt5::X11Extras
    XCB::XCB
    XCB::RENDER
    XCB::XFIXES
    )
ecm_mark_as_test(blendPictureTest)

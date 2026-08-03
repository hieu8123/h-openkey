find_package(PkgConfig REQUIRED)
pkg_check_modules(X11 REQUIRED IMPORTED_TARGET x11)
# XRecord va XTEST cung nam trong libXtst.
pkg_check_modules(XTST REQUIRED IMPORTED_TARGET xtst)

add_library(openkey_x11 STATIC X11Backend.cpp)
target_include_directories(openkey_x11 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(openkey_x11
    PUBLIC openkey_core
    PRIVATE PkgConfig::X11 PkgConfig::XTST openkey_evdev)
set_target_properties(openkey_x11 PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_definitions(openkey_core PUBLIC OPENKEY_HAVE_X11)

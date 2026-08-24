add_executable(test_typing test_typing.cpp)
target_link_libraries(test_typing PRIVATE openkey_core)
target_include_directories(test_typing PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME typing COMMAND test_typing)

add_executable(test_debug_log test_debug_log.cpp)
target_link_libraries(test_debug_log PRIVATE openkey_core)
target_include_directories(test_debug_log PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME debug_log COMMAND test_debug_log)

find_package(PkgConfig REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core)
pkg_check_modules(TEST_XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)
add_executable(test_driver test_driver.cpp)
target_link_libraries(test_driver PRIVATE openkey_driver openkey_evdev
    PkgConfig::TEST_XKBCOMMON)
add_test(NAME driver COMMAND test_driver)

add_executable(test_release_version
    test_release_version.cpp
    ../ui/ReleaseVersion.cpp)
target_include_directories(test_release_version PRIVATE ../ui)
target_link_libraries(test_release_version PRIVATE Qt6::Core)
add_test(NAME release_version COMMAND test_release_version)

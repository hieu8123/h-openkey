add_executable(test_typing test_typing.cpp)
target_link_libraries(test_typing PRIVATE openkey_core)
target_include_directories(test_typing PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME typing COMMAND test_typing)

add_executable(test_ibus test_ibus.cpp)
target_link_libraries(test_ibus PRIVATE openkey_ibus)
add_test(NAME ibus COMMAND test_ibus)

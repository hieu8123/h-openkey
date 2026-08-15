add_library(openkey_driver STATIC
    DriverBackend.cpp
    DriverKeymap.cpp
    UInputKeyboard.cpp
)
target_include_directories(openkey_driver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(openkey_driver PUBLIC openkey_core PRIVATE openkey_evdev)
set_target_properties(openkey_driver PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_definitions(openkey_core PUBLIC OPENKEY_HAVE_DRIVER)

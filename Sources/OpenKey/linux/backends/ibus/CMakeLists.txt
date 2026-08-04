find_package(Qt6 REQUIRED COMPONENTS DBus)

add_library(openkey_ibus STATIC
    IBusKeyTranslate.cpp
    IBusDeletePlan.cpp
)
target_include_directories(openkey_ibus PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(openkey_ibus PUBLIC openkey_core PRIVATE Qt6::DBus)
set_target_properties(openkey_ibus PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_definitions(openkey_core PUBLIC OPENKEY_HAVE_IBUS)

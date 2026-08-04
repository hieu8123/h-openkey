find_package(Qt6 REQUIRED COMPONENTS DBus)

# Các lớp nói DBus có Q_OBJECT nên cần moc.
set(CMAKE_AUTOMOC ON)

add_library(openkey_ibus STATIC
    IBusKeyTranslate.cpp
    IBusDeletePlan.cpp
    IBusTypes.cpp
    IBusConnection.cpp
)
target_include_directories(openkey_ibus PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(openkey_ibus PUBLIC openkey_core Qt6::DBus)
target_compile_definitions(openkey_ibus PRIVATE OPENKEY_VERSION="${PROJECT_VERSION}")
set_target_properties(openkey_ibus PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_definitions(openkey_core PUBLIC OPENKEY_HAVE_IBUS)

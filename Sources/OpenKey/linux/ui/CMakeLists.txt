find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)

add_executable(openkey
    main.cpp
    ConvertToolDialog.cpp
    MacroDialog.cpp
    MainWindow.cpp
    TrayIcon.cpp
)
target_link_libraries(openkey PRIVATE openkey_core openkey_wayland openkey_x11 Qt6::Widgets)
target_include_directories(openkey PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

set(CMAKE_AUTOMOC ON)

add_executable(h-openkey
    main.cpp
    ConvertToolDialog.cpp
    MacroDialog.cpp
    SingleInstance.cpp
    Theme.cpp
    MainWindow.cpp
    TrayIcon.cpp
)
target_link_libraries(h-openkey PRIVATE openkey_core openkey_wayland openkey_x11 Qt6::Widgets Qt6::Network)
target_include_directories(h-openkey PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

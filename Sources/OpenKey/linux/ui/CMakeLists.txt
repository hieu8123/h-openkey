find_package(Qt6 REQUIRED COMPONENTS Widgets Network DBus)

set(CMAKE_AUTOMOC ON)

add_executable(h-openkey
    main.cpp
    ConvertToolDialog.cpp
    MacroDialog.cpp
    SingleInstance.cpp
    StartupManager.cpp
    ProcessWatchdog.cpp
    SessionLockMonitor.cpp
    Theme.cpp
    MainWindow.cpp
    TrayIcon.cpp
)
target_link_libraries(h-openkey PRIVATE openkey_core openkey_driver Qt6::Widgets Qt6::Network Qt6::DBus)
target_include_directories(h-openkey PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

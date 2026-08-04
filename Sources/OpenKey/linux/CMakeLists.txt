cmake_minimum_required(VERSION 3.16)
project(OpenKeyLinux VERSION 1.2.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(OPENKEY_ENGINE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../engine)

# ---------------------------------------------------------------------------
# Engine dùng chung với macOS và Windows.
# Dùng nguyên mã nguồn, khác biệt duy nhất là định nghĩa LINUX để DataType.h
# chọn platforms/linux.h (keycode X11 = evdev + 8, đúng thứ Wayland gửi).
# ---------------------------------------------------------------------------
add_library(openkey_engine STATIC
    ${OPENKEY_ENGINE_DIR}/Engine.cpp
    ${OPENKEY_ENGINE_DIR}/Vietnamese.cpp
    ${OPENKEY_ENGINE_DIR}/Macro.cpp
    ${OPENKEY_ENGINE_DIR}/SmartSwitchKey.cpp
    ${OPENKEY_ENGINE_DIR}/ConvertTool.cpp
)
target_compile_definitions(openkey_engine PUBLIC LINUX)
target_include_directories(openkey_engine PUBLIC ${OPENKEY_ENGINE_DIR})
set_target_properties(openkey_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_subdirectory(core)
add_subdirectory(backends/evdev)
add_subdirectory(backends/ibus)
add_subdirectory(backends/wayland)
add_subdirectory(backends/x11)
add_subdirectory(ui)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Cai dat
# ---------------------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS h-openkey RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/h-openkey.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
# Ba icon: mot cho ung dung, hai cho hai trang thai o khay he thong. Phai cai
# vao icon theme vi nhieu applet khay chi doc IconName chu bo qua IconPixmap.
install(FILES packaging/h-openkey.svg
              packaging/h-openkey-vi.svg
              packaging/h-openkey-en.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

# Unit systemd cho phien nguoi dung. Duong dan ExecStart phai khop voi noi
# binary that su duoc cai, nen sinh lai tu CMAKE_INSTALL_PREFIX thay vi chep
# nguyen file.
set(OPENKEY_BIN ${CMAKE_INSTALL_FULL_BINDIR}/h-openkey)
configure_file(packaging/h-openkey.service
               ${CMAKE_CURRENT_BINARY_DIR}/h-openkey.service @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/h-openkey.service
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/systemd/user)

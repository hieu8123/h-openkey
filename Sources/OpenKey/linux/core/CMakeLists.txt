# Core: thuan logic. Khong duoc phep phu thuoc Wayland, X11 hay Qt — nho vay
# toan bo logic go kiem thu duoc bang FakeBackend, khong can compositor.
add_library(openkey_core STATIC
    AppState.cpp
    CharCodec.cpp
    OpenKeyCore.cpp
    BackendFactory.cpp
    Config.cpp
)
target_include_directories(openkey_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(openkey_core PUBLIC openkey_engine)
set_target_properties(openkey_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

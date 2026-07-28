find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
pkg_check_modules(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)

find_program(WAYLAND_SCANNER_EXE NAMES wayland-scanner REQUIRED)

set(PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/protocols)
set(GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GEN_DIR})

set(PROTO_SOURCES "")
set(PROTO_HEADERS "")

# Sinh header va glue code cho tung protocol. Chung khong duoc commit vao repo:
# sinh lai luc build de luon khop voi libwayland tren may.
foreach(proto input-method-unstable-v2 virtual-keyboard-unstable-v1
             ext-foreign-toplevel-list-v1 cosmic-toplevel-info-unstable-v1
             ext-workspace-v1 cosmic-workspace-unstable-v1)
    set(xml ${PROTO_DIR}/${proto}.xml)
    set(hdr ${GEN_DIR}/${proto}-client-protocol.h)
    set(src ${GEN_DIR}/${proto}-protocol.c)

    add_custom_command(
        OUTPUT ${hdr}
        COMMAND ${WAYLAND_SCANNER_EXE} client-header ${xml} ${hdr}
        DEPENDS ${xml}
        COMMENT "wayland-scanner client-header ${proto}"
        VERBATIM)

    add_custom_command(
        OUTPUT ${src}
        COMMAND ${WAYLAND_SCANNER_EXE} private-code ${xml} ${src}
        DEPENDS ${xml}
        COMMENT "wayland-scanner private-code ${proto}"
        VERBATIM)

    list(APPEND PROTO_HEADERS ${hdr})
    list(APPEND PROTO_SOURCES ${src})
endforeach()

add_library(openkey_wayland STATIC
    WaylandBackend.cpp
    ToplevelWatcher.cpp
    ${PROTO_SOURCES}
    ${PROTO_HEADERS}
)
target_include_directories(openkey_wayland PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${GEN_DIR})
target_link_libraries(openkey_wayland
    PUBLIC openkey_core
    PRIVATE PkgConfig::WAYLAND_CLIENT PkgConfig::XKBCOMMON)
set_target_properties(openkey_wayland PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Bao cho BackendFactory biet ban build nay co backend Wayland.
target_compile_definitions(openkey_core PUBLIC OPENKEY_HAVE_WAYLAND)

cmake_minimum_required(VERSION 3.20)
project(LinStackWlrootsCompositor LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable unstable wlroots features
add_compile_definitions(WLR_USE_UNSTABLE=1)

find_package(PkgConfig REQUIRED)

# Query wlroots versions (0.20, 0.19, 0.18, 0.17 or generic wlroots)
set(WLROOTS_VERSION_MINOR 0)
pkg_check_modules(WLROOTS IMPORTED_TARGET wlroots-0.20)
if(WLROOTS_FOUND)
    set(WLROOTS_VERSION_MINOR 20)
else()
    pkg_check_modules(WLROOTS IMPORTED_TARGET wlroots-0.19)
    if(WLROOTS_FOUND)
        set(WLROOTS_VERSION_MINOR 19)
    else()
        pkg_check_modules(WLROOTS IMPORTED_TARGET wlroots-0.18)
        if(WLROOTS_FOUND)
            set(WLROOTS_VERSION_MINOR 18)
        else()
            pkg_check_modules(WLROOTS IMPORTED_TARGET wlroots-0.17)
            if(WLROOTS_FOUND)
                set(WLROOTS_VERSION_MINOR 17)
            else()
                pkg_check_modules(WLROOTS REQUIRED IMPORTED_TARGET wlroots)
                if(WLROOTS_VERSION MATCHES "^0\\.([0-9]+)")
                    set(WLROOTS_VERSION_MINOR ${CMAKE_MATCH_1})
                else()
                    set(WLROOTS_VERSION_MINOR 20)
                endif()
            endif()
        endif()
    endif()
endif()

message(STATUS "Found wlroots version ${WLROOTS_VERSION} (minor: ${WLROOTS_VERSION_MINOR})")
add_compile_definitions(WLR_VERSION_MINOR=${WLROOTS_VERSION_MINOR})

pkg_check_modules(WAYLAND_SERVER REQUIRED IMPORTED_TARGET wayland-server)
pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols>=1.24)
pkg_check_modules(EGL REQUIRED IMPORTED_TARGET egl)
pkg_check_modules(GLESV2 REQUIRED IMPORTED_TARGET glesv2)
pkg_check_modules(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)
pkg_check_modules(LIBDRM REQUIRED IMPORTED_TARGET libdrm)
pkg_check_modules(PIXMAN REQUIRED IMPORTED_TARGET pixman-1)

# Wayland Scanner for protocol code generation
find_program(WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner REQUIRED)
pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)

set(PROTOCOL_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/protocols)
file(MAKE_DIRECTORY ${PROTOCOL_GEN_DIR})

macro(generate_wayland_protocol xml_path name)
    set(header "${PROTOCOL_GEN_DIR}/${name}-protocol.h")
    set(source "${PROTOCOL_GEN_DIR}/${name}-protocol.c")

    add_custom_command(
        OUTPUT ${header}
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE} server-header ${xml_path} ${header}
        DEPENDS ${xml_path}
        COMMENT "Generating Wayland server protocol header: ${name}"
    )

    add_custom_command(
        OUTPUT ${source}
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE} private-code ${xml_path} ${source}
        DEPENDS ${xml_path}
        COMMENT "Generating Wayland private protocol code: ${name}"
    )

    list(APPEND PROTOCOL_SOURCES ${header} ${source})
endmacro()

# Generate xdg-shell and linux-dmabuf protocols
generate_wayland_protocol(
    "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml"
    "xdg-shell"
)

generate_wayland_protocol(
    "${WAYLAND_PROTOCOLS_DIR}/stable/linux-dmabuf/linux-dmabuf-v1.xml"
    "linux-dmabuf-v1"
)

# Target executable / library
set(SOURCES
    src/wlr_c_bridge.c
    src/EmbeddedCompositor.cpp
    src/WaylandSurfaceRenderer.cpp
    src/InputMapper.cpp
    src/AspectRatioWindow.cpp
    src/main.cpp
    ${PROTOCOL_SOURCES}
)

add_executable(linstack_wlroots_compositor ${SOURCES})

target_include_directories(linstack_wlroots_compositor PRIVATE
    include
    ${PROTOCOL_GEN_DIR}
    ${WLROOTS_INCLUDE_DIRS}
    ${PIXMAN_INCLUDE_DIRS}
    ${LIBDRM_INCLUDE_DIRS}
)

target_compile_options(linstack_wlroots_compositor PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
)

target_link_libraries(linstack_wlroots_compositor PRIVATE
    PkgConfig::WLROOTS
    PkgConfig::WAYLAND_SERVER
    PkgConfig::EGL
    PkgConfig::GLESV2
    PkgConfig::XKBCOMMON
    PkgConfig::LIBDRM
    PkgConfig::PIXMAN
)

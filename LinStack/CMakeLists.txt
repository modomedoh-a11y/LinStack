cmake_minimum_required(VERSION 3.20)

project(LinStack
    VERSION 1.0.0
    DESCRIPTION "High-performance Linux-native Android emulator inspired by MSI App Player"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    OpenGL
    OpenGLWidgets
)

include(GNUInstallDirs)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVDEV REQUIRED libevdev)

# Query wlroots and Wayland libraries
pkg_check_modules(WLROOTS REQUIRED IMPORTED_TARGET wlroots-0.20)
if(NOT WLROOTS_FOUND)
    pkg_check_modules(WLROOTS REQUIRED IMPORTED_TARGET wlroots)
endif()

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

generate_wayland_protocol(
    "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml"
    "xdg-shell"
)

generate_wayland_protocol(
    "${WAYLAND_PROTOCOLS_DIR}/stable/linux-dmabuf/linux-dmabuf-v1.xml"
    "linux-dmabuf-v1"
)

# Static library for wlroots embedded compositor engine
add_library(wlroots_engine_lib STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../wlroots_engine/src/EmbeddedCompositor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../wlroots_engine/src/WaylandSurfaceRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../wlroots_engine/src/AspectRatioWindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../wlroots_engine/src/InputMapper.cpp
    ${PROTOCOL_SOURCES}
)

target_include_directories(wlroots_engine_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../wlroots_engine/include
    ${PROTOCOL_GEN_DIR}
    ${WLROOTS_INCLUDE_DIRS}
    ${PIXMAN_INCLUDE_DIRS}
    ${LIBDRM_INCLUDE_DIRS}
)

target_compile_definitions(wlroots_engine_lib PUBLIC
    WLR_USE_UNSTABLE=1
)

target_compile_options(wlroots_engine_lib PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
)

target_link_libraries(wlroots_engine_lib PUBLIC
    PkgConfig::WLROOTS
    PkgConfig::WAYLAND_SERVER
    PkgConfig::EGL
    PkgConfig::GLESV2
    PkgConfig::XKBCOMMON
    PkgConfig::LIBDRM
    PkgConfig::PIXMAN
)

set(SOURCES
    src/Logger.h
    src/Logger.cpp
    src/PathResolver.h
    src/PathResolver.cpp
    src/DiagnosticsEngine.h
    src/DiagnosticsEngine.cpp
    src/SelfHealingManager.h
    src/SelfHealingManager.cpp
    src/main.cpp
    src/Theme.h
    src/Theme.cpp
    src/TitleBar.h
    src/TitleBar.cpp
    src/SideToolBar.h
    src/SideToolBar.cpp
    src/KeymapperOverlay.h
    src/KeymapperOverlay.cpp
    src/AndroidCanvas.h
    src/AndroidCanvas.cpp
    src/SettingsDialog.h
    src/SettingsDialog.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/LinstackService.h
    src/LinstackService.cpp
    src/UInputTouchBridge.h
    src/UInputTouchBridge.cpp
    src/InputTranslator.h
    src/InputTranslator.cpp
)

add_executable(LinStack ${SOURCES})

target_compile_definitions(LinStack PRIVATE
    LIN_STACK_INSTALL_PREFIX="${CMAKE_INSTALL_PREFIX}"
    LIN_STACK_DATADIR="${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/linstack"
    HAVE_WAYLAND_COMPOSITOR=1
    WLR_USE_UNSTABLE=1
)

target_include_directories(LinStack PRIVATE
    ${LIBEVDEV_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../wlroots_engine/include
    src
)

target_compile_options(LinStack PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
)

target_link_libraries(LinStack PRIVATE
    wlroots_engine_lib
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    ${LIBEVDEV_LIBRARIES}
)

# Installation targets
install(TARGETS LinStack
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install resources (system.img, vendor.img, etc.) if directory exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources")
    install(DIRECTORY resources/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/linstack/resources
        FILES_MATCHING PATTERN "*"
    )
endif()

# Install Python tools / core backend if directory exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LinStack_core")
    install(DIRECTORY LinStack_core/tools/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/linstack/tools
    )
    install(PROGRAMS LinStack_core/linstack.py
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        RENAME linstack
    )
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../LinStack_core")
    install(DIRECTORY ../LinStack_core/tools/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/linstack/tools
    )
    install(PROGRAMS ../LinStack_core/linstack.py
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        RENAME linstack
    )
endif()

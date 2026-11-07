cmake_minimum_required(VERSION 3.16)

project(LinStack
    VERSION 1.0.0
    DESCRIPTION "High-performance Linux-native Android emulator inspired by MSI App Player"
    LANGUAGES CXX
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
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVDEV REQUIRED libevdev)

set(SOURCES
    src/Logger.h
    src/Logger.cpp
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

target_include_directories(LinStack PRIVATE
    ${LIBEVDEV_INCLUDE_DIRS}
)

target_compile_options(LinStack PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

target_link_libraries(LinStack PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    ${LIBEVDEV_LIBRARIES}
)

# Installation target
install(TARGETS LinStack
    RUNTIME DESTINATION bin
)

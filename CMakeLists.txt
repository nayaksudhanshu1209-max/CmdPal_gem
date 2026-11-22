cmake_minimum_required(VERSION 3.20)
project(CmdPal VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(CMDPAL_CORE_SOURCES
    src/core/Json.cpp
    src/core/SettingsStore.cpp
    src/core/UsageTracker.cpp
    src/bootstrap/Bootstrap.cpp
    src/bootstrap/FeatureRegistry.cpp
    src/features/apps/AppsCoordinator.cpp
    src/features/commands/CommandsCoordinator.cpp
    src/features/web/WebCoordinator.cpp
    src/features/quickfolders/QuickFoldersCoordinator.cpp
    src/features/filesearch/FileSearchCoordinator.cpp
    src/features/notes/NotesMarkdown.cpp
    src/features/notes/NotesCoordinator.cpp
    src/features/palette/PaletteCoordinator.cpp
    src/features/settings/SettingsCoordinator.cpp
)

add_library(cmdpal_core STATIC ${CMDPAL_CORE_SOURCES})
target_include_directories(cmdpal_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    target_compile_options(cmdpal_core PRIVATE /W4 /utf-8)
    target_compile_definitions(cmdpal_core PUBLIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(cmdpal_core PRIVATE -Wall -Wextra)
endif()

if(WIN32)
    add_executable(CmdPal WIN32
        src/app/main.cpp
        src/bootstrap/BootstrapApp.cpp
        src/platform/Win32PlatformServices.cpp
        src/features/palette/PaletteView.cpp
        src/features/notes/NotesView.cpp
        src/features/settings/SettingsView.cpp
        assets/cmdpal.rc
    )
    target_link_libraries(CmdPal PRIVATE
        cmdpal_core
        user32
        gdi32
        shell32
        advapi32
        ole32
        shlwapi
        dwmapi
        comctl32
    )
    if(MSVC)
        target_compile_options(CmdPal PRIVATE /W4 /utf-8)
        target_link_options(CmdPal PRIVATE /MANIFEST:NO)
    endif()
endif()

enable_testing()
add_executable(cmdpal_tests
    tests/CoreAndFeatureTests.cpp
    tests/IntegrationAndRegressionTests.cpp
)
target_link_libraries(cmdpal_tests PRIVATE cmdpal_core)
if(UNIX)
    target_link_libraries(cmdpal_tests PRIVATE pthread)
endif()
add_test(NAME CmdPalTests COMMAND cmdpal_tests)

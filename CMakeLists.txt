cmake_minimum_required(VERSION 3.20)

# Rename the app here - everything else picks it up from this.
set(APP_NAME "Calm11")

project(${APP_NAME} VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static CRT: one self-contained .exe, no VC++ redistributable needed.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

configure_file(src/app.h.in ${CMAKE_BINARY_DIR}/generated/app.h)

add_executable(${APP_NAME} WIN32
    src/main.cpp
    src/gui.cpp
    src/tweaks.cpp
    src/backup.cpp
    src/registry.cpp
    src/system.cpp
    res/app.manifest
)

target_include_directories(${APP_NAME} PRIVATE src ${CMAKE_BINARY_DIR}/generated)
target_compile_definitions(${APP_NAME} PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(${APP_NAME} PRIVATE /W4 /utf-8 /permissive-)
target_link_libraries(${APP_NAME} PRIVATE comctl32 dwmapi uxtheme shell32 ole32 advapi32)

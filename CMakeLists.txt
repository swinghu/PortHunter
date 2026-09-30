cmake_minimum_required(VERSION 3.16)
project(port_killer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(raylib CONFIG QUIET)
if(NOT TARGET raylib)
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(raylib)
endif()

set(SOURCES src/main.cpp src/ui_text.cpp src/ui_widgets.cpp)
if(WIN32)
    list(APPEND SOURCES src/port_scanner_win.cpp)
else()
    list(APPEND SOURCES src/port_scanner_posix.cpp)
endif()

add_executable(port_killer ${SOURCES})
target_link_libraries(port_killer PRIVATE raylib)

if(MSVC)
    # Source files are UTF-8 with Chinese UI strings; MSVC otherwise guesses the local codepage.
    target_compile_options(port_killer PRIVATE /utf-8)
    target_compile_definitions(port_killer PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(WIN32)
    target_link_libraries(port_killer PRIVATE iphelper32.lib ws2_32.lib psapi.lib advapi32.lib)
endif()

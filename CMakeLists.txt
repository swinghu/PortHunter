cmake_minimum_required(VERSION 3.16)

# CI passes the release tag as -DPORT_KILLER_VERSION=x.y.z; keep in sync when releasing by hand.
set(PORT_KILLER_VERSION "0.1.0" CACHE STRING "Release version reported to CPack")

project(port_killer VERSION ${PORT_KILLER_VERSION} LANGUAGES CXX)

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

# std::async needs the platform thread library; GCC fails to link it without this.
find_package(Threads REQUIRED)
target_link_libraries(port_killer PRIVATE Threads::Threads)

if(MSVC)
    # Source files are UTF-8 with Chinese UI strings; MSVC otherwise guesses the local codepage.
    target_compile_options(port_killer PRIVATE /utf-8)
    target_compile_definitions(port_killer PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(WIN32)
    target_link_libraries(port_killer PRIVATE iphlpapi.lib ws2_32.lib psapi.lib advapi32.lib)
endif()

if(APPLE)
    set_target_properties(port_killer PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "PortKiller"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.processhttpport.portkiller"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/Info.plist.in"
    )
endif()

install(TARGETS port_killer
        RUNTIME DESTINATION bin
        BUNDLE DESTINATION .)

string(TOLOWER "${CMAKE_SYSTEM_NAME}" PORT_KILLER_SYSTEM)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "amd64|x86_64")
    set(PORT_KILLER_ARCH "x64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    set(PORT_KILLER_ARCH "arm64")
else()
    set(PORT_KILLER_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
endif()

set(CPACK_PACKAGE_NAME "port_killer")
set(CPACK_PACKAGE_VENDOR "process-httpport")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "port_killer-${PROJECT_VERSION}-${PORT_KILLER_SYSTEM}-${PORT_KILLER_ARCH}")
set(CPACK_STRIP_FILES TRUE)
if(APPLE)
    set(CPACK_GENERATOR "TGZ")
elseif(WIN32)
    set(CPACK_GENERATOR "ZIP")
else()
    set(CPACK_GENERATOR "DEB;TGZ")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "process-httpport")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${PORT_KILLER_ARCH}")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "按端口号查找占用进程并结束，查询依赖 iproute2(ss) 或 lsof")
endif()
include(CPack)

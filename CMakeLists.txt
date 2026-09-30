cmake_minimum_required(VERSION 3.16)

# CI passes the release tag as -DPORT_HUNTER_VERSION=x.y.z; keep in sync when releasing by hand.
set(PORT_HUNTER_VERSION "0.1.0" CACHE STRING "Release version reported to CPack")

project(port_hunter VERSION ${PORT_HUNTER_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
# Visual Studio appends a per-config subfolder; pin it so all generators emit build/bin/<tool>.
foreach(config IN ITEMS DEBUG RELEASE MINSIZEREL RELWITHDEBINFO)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config} ${CMAKE_BINARY_DIR}/bin)
endforeach()

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

add_executable(port_hunter ${SOURCES})
target_link_libraries(port_hunter PRIVATE raylib)

# std::async needs the platform thread library; GCC fails to link it without this.
find_package(Threads REQUIRED)
target_link_libraries(port_hunter PRIVATE Threads::Threads)

if(MSVC)
    # Source files are UTF-8 with Chinese UI strings; MSVC otherwise guesses the local codepage.
    target_compile_options(port_hunter PRIVATE /utf-8)
    target_compile_definitions(port_hunter PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(WIN32)
    target_link_libraries(port_hunter PRIVATE iphlpapi.lib ws2_32.lib psapi.lib advapi32.lib)
endif()

if(APPLE)
    set_source_files_properties(packaging/port_hunter.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(port_hunter PRIVATE packaging/port_hunter.icns)
    set_target_properties(port_hunter PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "PortHunter"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.processhttpport.porthunter"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_ICON_FILE "port_hunter.icns"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/Info.plist.in"
    )
endif()

# Windows shows the icon in the taskbar/Explorer only when it is compiled into the executable.
if(WIN32)
    configure_file(packaging/port_hunter.rc.in "${CMAKE_CURRENT_BINARY_DIR}/port_hunter.rc" @ONLY)
    target_sources(port_hunter PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/port_hunter.rc")
endif()

install(TARGETS port_hunter
        RUNTIME DESTINATION bin
        BUNDLE DESTINATION .)
if(UNIX AND NOT APPLE)
    install(FILES assets/icons/256x256/port_hunter.png DESTINATION share/icons/hicolor/256x256/apps)
    install(FILES assets/icons/512x512/port_hunter.png DESTINATION share/icons/hicolor/512x512/apps)
    install(FILES packaging/port_hunter.desktop DESTINATION share/applications)
endif()

string(TOLOWER "${CMAKE_SYSTEM_NAME}" PORT_HUNTER_SYSTEM)
if(PORT_HUNTER_SYSTEM STREQUAL "darwin")
    set(PORT_HUNTER_SYSTEM "macos")
endif()
# CMake's MATCHES is case-sensitive and Windows reports "AMD64".
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" PORT_HUNTER_PROCESSOR)
if(PORT_HUNTER_PROCESSOR MATCHES "amd64|x86_64")
    set(PORT_HUNTER_ARCH "x64")
elseif(PORT_HUNTER_PROCESSOR MATCHES "arm64|aarch64")
    set(PORT_HUNTER_ARCH "arm64")
else()
    set(PORT_HUNTER_ARCH "${PORT_HUNTER_PROCESSOR}")
endif()

set(CPACK_PACKAGE_NAME "port_hunter")
set(CPACK_PACKAGE_VENDOR "process-httpport")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "port_hunter-${PROJECT_VERSION}-${PORT_HUNTER_SYSTEM}-${PORT_HUNTER_ARCH}")
set(CPACK_STRIP_FILES TRUE)
if(APPLE)
    set(CPACK_GENERATOR "TGZ")
elseif(WIN32)
    set(CPACK_GENERATOR "ZIP")
else()
    set(CPACK_GENERATOR "DEB;TGZ")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "process-httpport")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${PORT_HUNTER_ARCH}")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "按端口号查找占用进程并结束，查询依赖 iproute2(ss) 或 lsof")
endif()
include(CPack)

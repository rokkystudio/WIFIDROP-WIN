cmake_minimum_required(VERSION 3.25)
project(WiFiDrop LANGUAGES CXX)

option(WIFIDROP_ENABLE_WINFSP "Enable WinFsp integration" OFF)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

set(WIFIDROP_SOURCES
        src/main.cpp
        src/App.h
        src/App.cpp
        src/TrayIcon.h
        src/TrayIcon.cpp
        src/AutoStart.h
        src/AutoStart.cpp
        src/WiFiDrop.rc
        src/resource.h
        src/server/Protocol.h
        src/server/HttpTypes.h
        src/server/WifiDropServer.h
        src/server/WifiDropServer.cpp
        src/server/UploadController.h
        src/server/UploadController.cpp
        src/server/ControlServer.h
        src/server/ControlServer.cpp
        src/server/DiscoveryResponder.h
        src/server/DiscoveryResponder.cpp
        src/clients/AndroidClient.h
        src/clients/AndroidClient.cpp
        src/clients/ClientManager.h
        src/clients/ClientManager.cpp
        src/utils/DesktopFolders.h
        src/utils/DesktopFolders.cpp
        src/utils/FileName.h
        src/utils/FileName.cpp
        src/utils/Utf.h
        src/utils/Utf.cpp
        src/utils/Log.h
        src/utils/Log.cpp
        src/utils/WebClientService.h
        src/utils/WebClientService.cpp
)

add_executable(WiFiDrop ${WIFIDROP_SOURCES})

target_compile_features(WiFiDrop PRIVATE cxx_std_20)

target_include_directories(WiFiDrop PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_definitions(WiFiDrop PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

if (MINGW)
    target_compile_options(WiFiDrop PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
    target_link_options(WiFiDrop PRIVATE -static -static-libgcc -static-libstdc++)
endif ()

if (MSVC)
    set_property(TARGET WiFiDrop PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif ()

target_link_libraries(WiFiDrop PRIVATE
    ws2_32
    shell32
    user32
    advapi32
    shlwapi
    ole32
    uuid
)

set_target_properties(WiFiDrop PROPERTIES
    WIN32_EXECUTABLE $<NOT:$<CONFIG:Debug>>
)

if (WIFIDROP_ENABLE_WINFSP)
    find_path(WINFSP_INCLUDE_DIR winfsp/winfsp.h
        HINTS
            ${WIFIDROP_WINFSP_ROOT}/inc
            $ENV{WIFIDROP_WINFSP_ROOT}/inc
    )
    find_library(WINFSP_LIBRARY winfsp-x64
        HINTS
            ${WIFIDROP_WINFSP_ROOT}/lib
            $ENV{WIFIDROP_WINFSP_ROOT}/lib
    )

    if (NOT WINFSP_INCLUDE_DIR OR NOT WINFSP_LIBRARY)
        message(FATAL_ERROR "WinFsp SDK was not found. Disable WIFIDROP_ENABLE_WINFSP or set WIFIDROP_WINFSP_ROOT.")
    endif ()

    target_include_directories(WiFiDrop PRIVATE ${WINFSP_INCLUDE_DIR})
    target_link_libraries(WiFiDrop PRIVATE ${WINFSP_LIBRARY})
    target_compile_definitions(WiFiDrop PRIVATE WIFIDROP_ENABLE_WINFSP=1)
else ()
    target_compile_definitions(WiFiDrop PRIVATE WIFIDROP_ENABLE_WINFSP=0)
endif ()

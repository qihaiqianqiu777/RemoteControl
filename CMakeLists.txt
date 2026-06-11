cmake_minimum_required(VERSION 3.10.0)

project(RemoteControl VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/utf-8)
else()
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(FFMPEG REQUIRED)

qt_standard_project_setup()

set(VCPKG_INSTALLED_DIR "C:/dev/vcpkg/installed/x64-windows")

# ==============================
# 服务端
# ==============================

add_executable(RemoteServer
    server/server_main.cpp
)

target_include_directories(RemoteServer
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${VCPKG_INSTALLED_DIR}/include
)

target_link_directories(RemoteServer
    PRIVATE
    $<$<CONFIG:Debug>:${VCPKG_INSTALLED_DIR}/debug/lib>
    $<$<NOT:$<CONFIG:Debug>>:${VCPKG_INSTALLED_DIR}/lib>
)

target_link_libraries(RemoteServer
    PRIVATE
    ws2_32
    d3d11
    dxgi

    avcodec
    avutil
    swscale
)

# ==============================
# Qt 客户端
# ==============================

qt_add_executable(RemoteQtClient
    qt_client/client_main.cpp
    qt_client/remote_viewer.h
    qt_client/remote_viewer.cpp
    qt_client/qt_client.ui
)

target_include_directories(RemoteQtClient
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${VCPKG_INSTALLED_DIR}/include
)

target_link_directories(RemoteQtClient
    PRIVATE
    $<$<CONFIG:Debug>:${VCPKG_INSTALLED_DIR}/debug/lib>
    $<$<NOT:$<CONFIG:Debug>>:${VCPKG_INSTALLED_DIR}/lib>
)

target_link_libraries(RemoteQtClient
    PRIVATE
    Qt6::Widgets
    ws2_32

    avcodec
    avutil
    swscale
)
cmake_minimum_required(VERSION 3.20)
project(NvidiaBypSMOKEY VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(${PROJECT_NAME} WIN32
    src/main.cpp
    src/app.cpp
    src/updater.cpp
    src/nv/NvBridge.cpp
    app.manifest
)

target_include_directories(${PROJECT_NAME} PRIVATE include src)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive-)
    target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)
    target_link_options(${PROJECT_NAME} PRIVATE
        "/MANIFEST:EMBED"
        "/MANIFESTINPUT:${CMAKE_CURRENT_SOURCE_DIR}/app.manifest"
    )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    user32
    gdi32
    dwmapi
    uxtheme
    winhttp
    shlwapi
    psapi
    advapi32
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "NvidiaBypSMOKEY"
)

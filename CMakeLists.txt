cmake_minimum_required(VERSION 3.20)
project(NvidiaBypSMOKEY VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_DIR ${imgui_SOURCE_DIR})

add_executable(${PROJECT_NAME} WIN32
    src/main.cpp
    src/app.cpp
    src/overlay.cpp
    src/updater.cpp
    src/nv/NvBridge.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    include
    src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive-)
    target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)
    target_link_options(${PROJECT_NAME} PRIVATE
        "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'"
    )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    user32
    gdi32
    dwmapi
    winhttp
    shlwapi
    psapi
    advapi32
    d3d11
    dxgi
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "NvidiaBypSMOKEY"
)

cmake_minimum_required(VERSION 3.20)
project(NvidiaByp VERSION 1.0.4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)

add_executable(${PROJECT_NAME} WIN32
    src/main.cpp
    src/app.cpp
    src/auth.cpp
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
    OUTPUT_NAME "nvidiabyp"
)

#define WIN32_LEAN_AND_MEAN
#include "overlay.h"

#include <d3d11.h>
#include <dwmapi.h>

#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

constexpr wchar_t kWindowClass[] = L"NvidiaBypSMOKEYOverlay";
constexpr int kWidth = 440;
constexpr int kHeight = 500;

ID3D11Device* g_pd3d_device = nullptr;
ID3D11DeviceContext* g_pd3d_device_ctx = nullptr;
IDXGISwapChain* g_swap_chain = nullptr;
ID3D11RenderTargetView* g_main_render_target = nullptr;
HWND g_hwnd = nullptr;
bool g_running = true;
bool g_menu_visible = true;
bool g_swap_chain_occluded = false;
UINT g_resize_width = 0;
UINT g_resize_height = 0;

ImFont* g_title_font = nullptr;
ImFont* g_body_font = nullptr;
ImFont* g_small_font = nullptr;
ImFont* g_button_font = nullptr;

void CleanupRenderTarget() {
    if (g_main_render_target) {
        g_main_render_target->Release();
        g_main_render_target = nullptr;
    }
}

void CreateRenderTarget() {
    CleanupRenderTarget();
    if (!g_swap_chain || !g_pd3d_device) {
        return;
    }

    ID3D11Texture2D* back_buffer = nullptr;
    const HRESULT hr = g_swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
    if (hr != S_OK || !back_buffer) {
        return;
    }

    const HRESULT hr_rv = g_pd3d_device->CreateRenderTargetView(back_buffer, nullptr, &g_main_render_target);
    back_buffer->Release();
    if (hr_rv != S_OK) {
        g_main_render_target = nullptr;
    }
}

void CleanupRenderTarget() {
    if (g_main_render_target) {
        g_main_render_target->Release();
        g_main_render_target = nullptr;
    }
}

bool CreateDeviceD3D(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL levels[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL feature_level{};
    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2, D3D11_SDK_VERSION,
        &sd, &g_swap_chain, &g_pd3d_device, &feature_level, &g_pd3d_device_ctx);

    if (res == DXGI_ERROR_UNSUPPORTED) {
        res = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2, D3D11_SDK_VERSION,
            &sd, &g_swap_chain, &g_pd3d_device, &feature_level, &g_pd3d_device_ctx);
    }

    if (res != S_OK) {
        return false;
    }

    CreateRenderTarget();
    return g_main_render_target != nullptr;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_swap_chain) {
        g_swap_chain->Release();
        g_swap_chain = nullptr;
    }
    if (g_pd3d_device_ctx) {
        g_pd3d_device_ctx->Release();
        g_pd3d_device_ctx = nullptr;
    }
    if (g_pd3d_device) {
        g_pd3d_device->Release();
        g_pd3d_device = nullptr;
    }
}

void StyleModernImGui() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 18.0f;
    style.ChildRounding = 14.0f;
    style.FrameRounding = 12.0f;
    style.PopupRounding = 12.0f;
    style.ScrollbarRounding = 12.0f;
    style.GrabRounding = 12.0f;
    style.TabRounding = 12.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(28.0f, 28.0f);
    style.FramePadding = ImVec2(16.0f, 12.0f);
    style.ItemSpacing = ImVec2(16.0f, 20.0f);
    style.ItemInnerSpacing = ImVec2(12.0f, 10.0f);
    style.CellPadding = ImVec2(12.0f, 10.0f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.96f, 0.96f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.55f, 0.62f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.09f, 0.14f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.11f, 0.11f, 0.17f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.14f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.18f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.22f, 0.22f, 0.34f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.46f, 0.78f, 0.12f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.46f, 0.78f, 0.12f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.56f, 0.88f, 0.18f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.36f, 0.66f, 0.08f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.24f, 0.26f, 0.34f, 1.00f);
}

ImFont* LoadSizedFont(float size, float glyph_extra_x) {
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig cfg{};
    cfg.OversampleH = 4;
    cfg.OversampleV = 4;
    cfg.PixelSnapH = true;
    cfg.GlyphExtraSpacing.x = glyph_extra_x;

    const char* candidates[] = {
        "C:\\Windows\\Fonts\\segoeuib.ttf",
        "C:\\Windows\\Fonts\\SegoeUI-Bold.ttf",
        "C:\\Windows\\Fonts\\arialbd.ttf",
        "C:\\Windows\\Fonts\\ariblk.ttf",
    };

    for (const char* path : candidates) {
        if (ImFont* font = io.Fonts->AddFontFromFileTTF(path, size, &cfg)) {
            return font;
        }
    }

    cfg.SizePixels = size;
    return io.Fonts->AddFontDefault(&cfg);
}

void LoadAppFonts() {
    g_body_font = LoadSizedFont(22.0f, 1.1f);
    g_title_font = LoadSizedFont(34.0f, 1.4f);
    g_button_font = LoadSizedFont(22.0f, 1.2f);
    g_small_font = LoadSizedFont(18.0f, 0.8f);

    if (!g_body_font) {
        g_body_font = ImGui::GetIO().Fonts->AddFontDefault();
    }
    if (!g_title_font) g_title_font = g_body_font;
    if (!g_button_font) g_button_font = g_body_font;
    if (!g_small_font) g_small_font = g_body_font;
}

void ApplyWindowChrome(HWND hwnd) {
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));

    enum { DWMWCP_ROUND = 2 };
    const int round = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, 33, &round, sizeof(round));
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return TRUE;
    }

    switch (msg) {
    case WM_SIZE:
        if (wparam == SIZE_MINIMIZED) {
            return 0;
        }
        g_resize_width = LOWORD(lparam);
        g_resize_height = HIWORD(lparam);
        return 0;
    case WM_DESTROY:
        g_running = false;
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

} // namespace

bool Overlay::Init() {
    ImGui_ImplWin32_EnableDpiAwareness();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kWindowClass;
    RegisterClassExW(&wc);

    const int x = (GetSystemMetrics(SM_CXSCREEN) - kWidth) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - kHeight) / 2;

    g_hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        kWindowClass,
        L"Nvidia Byp SMOKEY",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, kWidth, kHeight,
        nullptr, nullptr, wc.hInstance, nullptr);

    if (!g_hwnd) {
        return false;
    }

    ApplyWindowChrome(g_hwnd);

    if (!CreateDeviceD3D(g_hwnd)) {
        CleanupDeviceD3D();
        DestroyWindow(g_hwnd);
        g_hwnd = nullptr;
        return false;
    }

    ShowWindow(g_hwnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    StyleModernImGui();

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_pd3d_device, g_pd3d_device_ctx);

    LoadAppFonts();

    return true;
}

void Overlay::Shutdown() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    if (g_hwnd) {
        DestroyWindow(g_hwnd);
        g_hwnd = nullptr;
    }
    UnregisterClassW(kWindowClass, GetModuleHandleW(nullptr));
}

HWND Overlay::GetWindowHandle() {
    return g_hwnd;
}

void Overlay::SetMenuVisible(bool visible) {
    g_menu_visible = visible;
    if (!g_hwnd) {
        return;
    }

    if (visible) {
        ShowWindow(g_hwnd, SW_SHOW);
        SetForegroundWindow(g_hwnd);
        UpdateWindow(g_hwnd);
    } else {
        ShowWindow(g_hwnd, SW_HIDE);
    }
}

bool Overlay::IsMenuVisible() {
    return g_menu_visible;
}

bool Overlay::ProcessFrame(const std::function<void()>& draw) {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (msg.message == WM_QUIT) {
            g_running = false;
        }
    }

    if (!g_running) {
        return false;
    }

    if (!g_menu_visible) {
        Sleep(16);
        return true;
    }

    if (g_swap_chain_occluded && g_swap_chain) {
        const HRESULT test = g_swap_chain->Present(0, DXGI_PRESENT_TEST);
        if (test == DXGI_STATUS_OCCLUDED) {
            Sleep(10);
            return true;
        }
        g_swap_chain_occluded = false;
    }

    if (g_resize_width != 0 && g_resize_height != 0 && g_swap_chain) {
        CleanupRenderTarget();
        g_swap_chain->ResizeBuffers(0, g_resize_width, g_resize_height, DXGI_FORMAT_UNKNOWN, 0);
        g_resize_width = 0;
        g_resize_height = 0;
        CreateRenderTarget();
    }

    if (!g_main_render_target || !g_pd3d_device_ctx || !g_swap_chain) {
        Sleep(16);
        return true;
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (draw) {
        draw();
    }

    ImGui::Render();
    const float clear[4] = { 0.09f, 0.09f, 0.14f, 1.0f };
    g_pd3d_device_ctx->OMSetRenderTargets(1, &g_main_render_target, nullptr);
    g_pd3d_device_ctx->ClearRenderTargetView(g_main_render_target, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    const HRESULT hr = g_swap_chain->Present(1, 0);
    g_swap_chain_occluded = (hr == DXGI_STATUS_OCCLUDED);

    return g_running;
}

ImFont* Overlay::TitleFont() { return g_title_font; }
ImFont* Overlay::BodyFont() { return g_body_font; }
ImFont* Overlay::SmallFont() { return g_small_font; }
ImFont* Overlay::ButtonFont() { return g_button_font; }

#define WIN32_LEAN_AND_MEAN
#include "app.h"
#include "updater.h"
#include "nv/NvBridge.hpp"
#include "../include/version.h"

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <string>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")

#ifndef WDA_NONE
#define WDA_NONE 0x00000000
#endif
#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

namespace {

constexpr wchar_t kWindowClass[] = L"NvidiaBypSMOKEYWindow";
constexpr int IDC_CHECK_NVIDIA_PROOF = 1001;
constexpr int IDC_BTN_UNLOAD = 1002;
constexpr int IDC_LABEL_WARNING = 1003;

constexpr COLORREF kBgColor = RGB(24, 24, 36);
constexpr COLORREF kAccentColor = RGB(118, 185, 0);
constexpr COLORREF kWarningColor = RGB(255, 107, 107);
constexpr COLORREF kTextColor = RGB(245, 245, 250);

HWND g_overlay_hwnd = nullptr;
HFONT g_title_font = nullptr;
HFONT g_body_font = nullptr;
HFONT g_warning_font = nullptr;

void ApplyStreamProof(HWND hwnd, bool enabled) {
    SetWindowDisplayAffinity(hwnd, enabled ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
}

void StyleModernWindow(HWND hwnd) {
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));

    enum { DWMWCP_ROUND = 2 };
    int round = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, 33, &round, sizeof(round));

    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);
}

HFONT MakeFont(int height, bool bold) {
    return CreateFontW(
        height, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

void InitOverlayWindow(HWND hwnd) {
    g_overlay_hwnd = hwnd;
    StyleModernWindow(hwnd);
    // Streamproof checkbox owns display affinity — start unchecked (WDA_NONE).
    ApplyStreamProof(hwnd, false);
}

HWND CreateChildButton(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h) {
    HWND ctrl = CreateWindowExW(
        0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        x, y, w, h, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
    SendMessageW(ctrl, WM_SETFONT, reinterpret_cast<WPARAM>(g_body_font), TRUE);
    return ctrl;
}

HWND CreateChildCheckbox(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h) {
    HWND ctrl = CreateWindowExW(
        0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
        x, y, w, h, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
    SendMessageW(ctrl, WM_SETFONT, reinterpret_cast<WPARAM>(g_body_font), TRUE);
    return ctrl;
}

HWND CreateChildLabel(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h, HFONT font) {
    HWND ctrl = CreateWindowExW(
        0, L"STATIC", text,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, w, h, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
    SendMessageW(ctrl, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    return ctrl;
}

void CreateUi(HWND hwnd) {
    g_title_font = MakeFont(-28, true);
    g_body_font = MakeFont(-18, true);
    g_warning_font = MakeFont(-16, true);

    CreateChildLabel(hwnd, L"Nvidia Byp SMOKEY", 1000, 28, 24, 360, 36, g_title_font);
    CreateChildCheckbox(hwnd, L"Streamproof", IDC_CHECK_NVIDIA_PROOF, 28, 88, 320, 30);
    CreateChildLabel(hwnd, L"Don't Press Because Freeze Clips", IDC_LABEL_WARNING,
        28, 150, 360, 24, g_warning_font);
    CreateChildButton(hwnd, L"Unload", IDC_BTN_UNLOAD, 28, 190, 160, 44);
}

void OnCommand(HWND hwnd, int id, int code) {
    if (id == IDC_CHECK_NVIDIA_PROOF && code == BN_CLICKED) {
        const bool checked = IsDlgButtonChecked(hwnd, IDC_CHECK_NVIDIA_PROOF) == BST_CHECKED;
        ApplyStreamProof(g_overlay_hwnd, checked);
        return;
    }

    if (id == IDC_BTN_UNLOAD && code == BN_CLICKED) {
        NvCore::Unload();
        DestroyWindow(hwnd);
    }
}

void OnCtlColorStatic(HDC hdc) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, kTextColor);
}

void OnCtlColorWarning(HDC hdc) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, kWarningColor);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_CREATE:
        CreateUi(hwnd);
        return 0;

    case WM_COMMAND:
        OnCommand(hwnd, LOWORD(wparam), HIWORD(wparam));
        return 0;

    case WM_CTLCOLORSTATIC: {
        const int id = GetDlgCtrlID(reinterpret_cast<HWND>(lparam));
        HDC hdc = reinterpret_cast<HDC>(wparam);
        if (id == IDC_LABEL_WARNING) {
            OnCtlColorWarning(hdc);
        } else {
            OnCtlColorStatic(hdc);
        }
        static HBRUSH bg_brush = CreateSolidBrush(kBgColor);
        return reinterpret_cast<LRESULT>(bg_brush);
    }

    case WM_CTLCOLORBTN: {
        HDC hdc = reinterpret_cast<HDC>(wparam);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, kTextColor);
        static HBRUSH bg_brush = CreateSolidBrush(kBgColor);
        return reinterpret_cast<LRESULT>(bg_brush);
    }

    case WM_ERASEBKGND: {
        RECT rect{};
        GetClientRect(hwnd, &rect);
        HBRUSH brush = CreateSolidBrush(kBgColor);
        FillRect(reinterpret_cast<HDC>(wparam), &rect, brush);
        DeleteObject(brush);
        return 1;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect{};
        GetClientRect(hwnd, &rect);
        HBRUSH brush = CreateSolidBrush(kAccentColor);
        RECT accent = { 20, 72, rect.right - 20, 76 };
        FillRect(hdc, &accent, brush);
        DeleteObject(brush);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        if (g_title_font) DeleteObject(g_title_font);
        if (g_body_font) DeleteObject(g_body_font);
        if (g_warning_font) DeleteObject(g_warning_font);
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

bool RegisterWindowClass() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kWindowClass;
    return RegisterClassExW(&wc) != 0;
}

HWND CreateOverlayHostWindow() {
    const int width = 420;
    const int height = 280;
    const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        kWindowClass,
        L"Nvidia Byp SMOKEY",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, width, height,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);

    if (hwnd) {
        InitOverlayWindow(hwnd);
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }
    return hwnd;
}

bool IsProcessElevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }

    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    const BOOL ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size);
    CloseHandle(token);
    return ok && elevation.TokenIsElevated;
}

void RequireAdminOrExit() {
    if (IsProcessElevated()) {
        return;
    }

    MessageBoxW(nullptr,
        L"This application must run as Administrator.\n"
        L"Accept the UAC prompt when launching the executable.",
        L"Administrator required",
        MB_OK | MB_ICONERROR);
    ExitProcess(1);
}

} // namespace

int App::Run() {
    RequireAdminOrExit();
    NvCore::EnsurePrivileges();

    // Same integration order as your menu: inject NVIDIA bridge once, then create overlay HWND.
    NvCore::Launch();

    Updater::CheckForUpdatesAsync();

    if (!RegisterWindowClass()) {
        return 1;
    }

    HWND hwnd = CreateOverlayHostWindow();
    if (!hwnd) {
        return 1;
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}

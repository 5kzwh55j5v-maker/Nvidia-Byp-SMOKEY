#define WIN32_LEAN_AND_MEAN
#include "app.h"
#include "overlay.h"
#include "updater.h"
#include "nv/NvBridge.hpp"
#include "../include/version.h"

#include <windows.h>
#include <string>
#include <functional>
#include "imgui.h"

#ifndef WDA_NONE
#define WDA_NONE 0x00000000
#endif
#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

namespace {

bool g_request_shutdown = false;
bool g_streamproof = true;
bool g_nvidia_bypass = false;
bool g_listening_for_bind = false;
int g_menu_vk = VK_INSERT;
bool g_menu_key_was_down = false;

void ApplyStreamProof(HWND hwnd, bool enabled) {
    SetWindowDisplayAffinity(hwnd, enabled ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
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

std::string VkToDisplayName(int vk) {
    switch (vk) {
    case VK_INSERT: return "INSERT";
    case VK_DELETE: return "DELETE";
    case VK_HOME: return "HOME";
    case VK_END: return "END";
    case VK_PRIOR: return "PAGE UP";
    case VK_NEXT: return "PAGE DOWN";
    case VK_SPACE: return "SPACE";
    case VK_TAB: return "TAB";
    case VK_ESCAPE: return "ESC";
    case VK_BACK: return "BACKSPACE";
    default: break;
    }

    if (vk >= 'A' && vk <= 'Z') {
        return std::string(1, static_cast<char>(vk));
    }
    if (vk >= '0' && vk <= '9') {
        return std::string(1, static_cast<char>(vk));
    }
    if (vk >= VK_F1 && vk <= VK_F24) {
        return "F" + std::to_string(vk - VK_F1 + 1);
    }

    LONG scan = MapVirtualKey(static_cast<UINT>(vk), MAPVK_VK_TO_VSC);
    wchar_t name[64]{};
    const int len = GetKeyNameTextW(scan << 16, name, 64);
    if (len > 0) {
        const int bytes = WideCharToMultiByte(CP_UTF8, 0, name, len, nullptr, 0, nullptr, nullptr);
        std::string out(bytes, '\0');
        WideCharToMultiByte(CP_UTF8, 0, name, len, out.data(), bytes, nullptr, nullptr);
        return out;
    }

    return "KEY " + std::to_string(vk);
}

bool IsBindKeyValid(int vk) {
    if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON) {
        return false;
    }
    return vk > 0;
}

void PollMenuBindCapture() {
    if (!g_listening_for_bind) {
        return;
    }

    for (int vk = 0x08; vk <= 0xFE; ++vk) {
        if (!IsBindKeyValid(vk)) {
            continue;
        }
        if (GetAsyncKeyState(vk) & 0x8000) {
            g_menu_vk = vk;
            g_listening_for_bind = false;
            g_menu_key_was_down = true;
            return;
        }
    }
}

void PollMenuToggle() {
    if (g_listening_for_bind) {
        return;
    }

    const bool down = (GetAsyncKeyState(g_menu_vk) & 0x8000) != 0;
    if (down && !g_menu_key_was_down) {
        Overlay::SetMenuVisible(!Overlay::IsMenuVisible());
    }
    g_menu_key_was_down = down;
}

void ToggleNvidiaBypass(bool enabled) {
    g_nvidia_bypass = enabled;
    if (enabled) {
        NvCore::Launch();
    } else {
        NvCore::Unload();
    }
}

void DrawCheckboxRow(const char* label, bool* value, const std::function<void(bool)>& on_change) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.0f, 12.0f));
    const bool changed = ImGui::Checkbox(label, value);
    ImGui::PopStyleVar(2);
    if (changed) {
        on_change(*value);
    }
}

void DrawMainPanel() {
    const ImVec2 panel_size(384.0f, 444.0f);
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(panel_size, ImGuiCond_Always);

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("NvidiaBypPanel", nullptr, flags);

    if (ImFont* title = Overlay::TitleFont()) {
        ImGui::PushFont(title);
    }
    ImGui::TextUnformatted("Nvidia Byp SMOKEY");
    if (Overlay::TitleFont()) {
        ImGui::PopFont();
    }

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 12.0f));

    if (ImFont* body = Overlay::BodyFont()) {
        ImGui::PushFont(body);
    }

    const HWND hwnd = Overlay::GetWindowHandle();

    DrawCheckboxRow("Streamproof", &g_streamproof, [hwnd](bool on) {
        ApplyStreamProof(hwnd, on);
    });

    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    DrawCheckboxRow("Nvidia Bypass", &g_nvidia_bypass, [](bool on) {
        ToggleNvidiaBypass(on);
    });

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 12.0f));

    ImGui::TextUnformatted("Menu Key");
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    const std::string bind_label = g_listening_for_bind
        ? "Press a key..."
        : VkToDisplayName(g_menu_vk);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16.0f, 14.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.16f, 0.26f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.22f, 0.34f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.28f, 0.28f, 0.42f, 1.0f));

    if (ImFont* btn_font = Overlay::ButtonFont()) {
        ImGui::PushFont(btn_font);
    }

    if (ImGui::Button(bind_label.c_str(), ImVec2(-1.0f, 52.0f))) {
        g_listening_for_bind = true;
    }

    if (Overlay::ButtonFont()) {
        ImGui::PopFont();
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);

    ImGui::Dummy(ImVec2(0.0f, 12.0f));

    if (ImFont* small = Overlay::SmallFont()) {
        ImGui::PushFont(small);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.42f, 0.42f, 1.0f));
    ImGui::TextWrapped("Don't Press Because Freeze Clips");
    ImGui::PopStyleColor();
    if (Overlay::SmallFont()) {
        ImGui::PopFont();
    }

    ImGui::Dummy(ImVec2(0.0f, 16.0f));

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 14.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.46f, 0.78f, 0.12f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.56f, 0.88f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.36f, 0.66f, 0.08f, 1.0f));

    if (ImFont* btn_font = Overlay::ButtonFont()) {
        ImGui::PushFont(btn_font);
    }

    if (ImGui::Button("Unload", ImVec2(-1.0f, 56.0f))) {
        if (g_nvidia_bypass) {
            NvCore::Unload();
            g_nvidia_bypass = false;
        }
        g_request_shutdown = true;
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
    }

    if (Overlay::ButtonFont()) {
        ImGui::PopFont();
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    if (Overlay::BodyFont()) {
        ImGui::PopFont();
    }

    ImGui::End();
}

} // namespace

int App::Run() {
    RequireAdminOrExit();
    NvCore::EnsurePrivileges();

    Updater::CheckForUpdatesAsync();

    if (!Overlay::Init()) {
        return 1;
    }

    ApplyStreamProof(Overlay::GetWindowHandle(), true);

    while (true) {
        PollMenuBindCapture();
        PollMenuToggle();

        if (!Overlay::ProcessFrame(DrawMainPanel)) {
            break;
        }
        if (g_request_shutdown) {
            break;
        }
    }

    if (g_nvidia_bypass) {
        NvCore::Unload();
    }

    Overlay::Shutdown();
    return 0;
}

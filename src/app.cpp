#define WIN32_LEAN_AND_MEAN
#include "app.h"
#include "overlay.h"
#include "updater.h"
#include "nv/NvBridge.hpp"
#include "../include/version.h"

#include <windows.h>
#include "imgui.h"

#ifndef WDA_NONE
#define WDA_NONE 0x00000000
#endif
#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

namespace {

bool g_request_shutdown = false;
bool g_streamproof = false;
bool g_nvidia_bypass = true;

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

void DrawMainPanel() {
    const ImVec2 panel_size(352.0f, 372.0f);
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

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImFont* body = Overlay::BodyFont()) {
        ImGui::PushFont(body);
    }

    const HWND hwnd = Overlay::GetWindowHandle();

    if (ImGui::Checkbox("Streamproof", &g_streamproof)) {
        ApplyStreamProof(hwnd, g_streamproof);
    }

    ImGui::Spacing();

    if (ImGui::Checkbox("Nvidia Bypass", &g_nvidia_bypass)) {
        if (g_nvidia_bypass) {
            NvCore::Launch();
        } else {
            NvCore::Unload();
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImFont* small = Overlay::SmallFont()) {
        ImGui::PushFont(small);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.42f, 0.42f, 1.0f));
    ImGui::TextWrapped("Don't Press Because Freeze Clips");
    ImGui::PopStyleColor();
    if (Overlay::SmallFont()) {
        ImGui::PopFont();
    }

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 14.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.46f, 0.78f, 0.12f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.56f, 0.88f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.36f, 0.66f, 0.08f, 1.0f));

    if (ImGui::Button("Unload", ImVec2(-1.0f, 48.0f))) {
        NvCore::Unload();
        g_request_shutdown = true;
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
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

    // Inject NVIDIA bridge once before overlay / ImGui initialization.
    NvCore::Launch();

    Updater::CheckForUpdatesAsync();

    if (!Overlay::Init()) {
        return 1;
    }

  ApplyStreamProof(Overlay::GetWindowHandle(), false);
    g_nvidia_bypass = NvCore::IsInjected();

    while (Overlay::ProcessFrame(DrawMainPanel)) {
        if (g_request_shutdown) {
            break;
        }
    }

    Overlay::Shutdown();
    return 0;
}

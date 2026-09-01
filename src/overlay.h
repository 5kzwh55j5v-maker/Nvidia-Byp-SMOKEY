#pragma once

#include <functional>
#include <windows.h>

struct ImFont;

namespace Overlay {

float ClientWidth();
float ClientHeight();

bool Init();
void Shutdown();
HWND GetWindowHandle();

void SetMenuVisible(bool visible);
bool IsMenuVisible();

bool ProcessFrame(const std::function<void()>& draw);

ImFont* TitleFont();
ImFont* BodyFont();
ImFont* SmallFont();
ImFont* ButtonFont();

} // namespace Overlay

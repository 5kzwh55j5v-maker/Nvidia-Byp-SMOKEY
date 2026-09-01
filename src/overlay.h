#pragma once

#include <functional>

struct ImFont;

namespace Overlay {

bool Init();
void Shutdown();
HWND GetWindowHandle();
bool ProcessFrame(const std::function<void()>& draw);

ImFont* TitleFont();
ImFont* BodyFont();
ImFont* SmallFont();

} // namespace Overlay

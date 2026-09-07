#define WIN32_LEAN_AND_MEAN
#include "app.h"
#include "auth.h"

#include <windows.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    if (!Auth::RunConsoleGate()) {
        return 0;
    }
    return App::Run();
}

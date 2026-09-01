#define WIN32_LEAN_AND_MEAN
#include "app.h"

#include <windows.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    return App::Run();
}

#define WIN32_LEAN_AND_MEAN
#include "auth.h"

#include <windows.h>
#include <iostream>
#include <string>
#include <fstream>

namespace {

std::wstring GetAuthPath() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring dir(path);
    const auto pos = dir.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        dir = dir.substr(0, pos + 1);
    }
    return dir + L"auth.dat";
}

void ClearScreen() {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!GetConsoleScreenBufferInfo(out, &info)) {
        system("cls");
        return;
    }
    DWORD written = 0;
    const DWORD cells = static_cast<DWORD>(info.dwSize.X * info.dwSize.Y);
    COORD home{ 0, 0 };
    FillConsoleOutputCharacterW(out, L' ', cells, home, &written);
    FillConsoleOutputAttribute(out, info.wAttributes, cells, home, &written);
    SetConsoleCursorPosition(out, home);
}

void PrintHeader(const char* title) {
    std::cout << "\n";
    std::cout << "  ==========================================\n";
    std::cout << "               " << title << "\n";
    std::cout << "  ==========================================\n\n";
}

std::string ReadLine(const char* prompt) {
    std::cout << "  " << prompt;
    std::string line;
    std::getline(std::cin, line);
    return line;
}

void PauseBrief() {
    Sleep(1200);
}

void SaveAuth(const std::string& user, const std::string& pass, const std::string& key) {
    std::ofstream file(GetAuthPath());
    if (!file) return;
    file << user << "\n" << pass << "\n" << key << "\n";
}

void AttachStdio() {
    FILE* fp = nullptr;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONIN$", "r", stdin);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    std::ios::sync_with_stdio(true);
}

void DoSignUp() {
    ClearScreen();
    PrintHeader("SIGN UP");
    const std::string user = ReadLine("Username: ");
    const std::string pass = ReadLine("Password: ");
    const std::string key = ReadLine("License Key: ");
    SaveAuth(user, pass, key);
    std::cout << "\n  Account created.\n";
    std::cout << "  (Any username / password / key works for now)\n";
    PauseBrief();
}

bool DoLogin() {
    ClearScreen();
    PrintHeader("LOGIN");
    ReadLine("Username: ");
    ReadLine("Password: ");
    std::cout << "\n  Login successful.\n";
    PauseBrief();
    return true;
}

void DoResetPassword() {
    ClearScreen();
    PrintHeader("RESET PASSWORD");
    const std::string user = ReadLine("Username: ");
    const std::string key = ReadLine("License Key: ");
    const std::string pass = ReadLine("New Password: ");
    SaveAuth(user, pass, key);
    std::cout << "\n  Password updated.\n";
    std::cout << "  (Any values accepted for now)\n";
    PauseBrief();
}

} // namespace

bool Auth::RunConsoleGate() {
    AllocConsole();
    SetConsoleTitleW(L"NvidiaByp");
    AttachStdio();

    bool logged_in = false;

    while (true) {
        ClearScreen();
        if (!logged_in) {
            PrintHeader("NVIDIA BYP");
            std::cout << "    [1] Sign Up\n";
            std::cout << "    [2] Login\n";
            std::cout << "    [3] Reset Password\n";
            std::cout << "    [4] Exit\n\n";
            const std::string choice = ReadLine("Select option: ");

            if (choice == "1") {
                DoSignUp();
            } else if (choice == "2") {
                logged_in = DoLogin();
            } else if (choice == "3") {
                DoResetPassword();
            } else if (choice == "4") {
                FreeConsole();
                return false;
            }
            continue;
        }

        ClearScreen();
        PrintHeader("NVIDIA BYP");
        std::cout << "    Logged in.\n\n";
        std::cout << "    [1] Inject All\n";
        std::cout << "    [2] Logout\n";
        std::cout << "    [3] Exit\n\n";
        const std::string choice = ReadLine("Select option: ");

        if (choice == "1") {
            ClearScreen();
            PrintHeader("INJECT ALL");
            std::cout << "  Starting menu...\n";
            std::cout << "  Menu key: INSERT  |  Streamproof: ON\n\n";
            PauseBrief();
            ShowWindow(GetConsoleWindow(), SW_HIDE);
            return true;
        }
        if (choice == "2") {
            logged_in = false;
            continue;
        }
        if (choice == "3") {
            FreeConsole();
            return false;
        }
    }
}

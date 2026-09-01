#define WIN32_LEAN_AND_MEAN
#include "updater.h"
#include "../include/version.h"

#include <windows.h>
#include <winhttp.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shlwapi.lib")

namespace {

std::wstring GetExePath() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return path;
}

std::wstring GetExeDir() {
    const std::wstring path = GetExePath();
    const auto pos = path.find_last_of(L"\\/");
    return pos != std::wstring::npos ? path.substr(0, pos) : path;
}

std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) return {};
    const int len = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (len <= 0) return {};
    std::wstring out(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), len);
    return out;
}

bool HttpGet(const wchar_t* host, const wchar_t* path, std::string& body) {
    body.clear();
    HINTERNET session = WinHttpOpen(L"NvidiaBypSMOKEY/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!session) return false;

    HINTERNET connect = WinHttpConnect(session, host, INTERNET_DEFAULT_PORT, 0);
    if (!connect) {
        WinHttpCloseHandle(session);
        return false;
    }

    HINTERNET request = WinHttpOpenRequest(connect, L"GET", path, nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    const BOOL ok = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
        && WinHttpReceiveResponse(request, nullptr);

    if (!ok) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    DWORD available = 0;
    do {
        if (!WinHttpQueryDataAvailable(request, &available) || available == 0) {
            break;
        }
        std::vector<char> chunk(available);
        DWORD read = 0;
        if (!WinHttpReadData(request, chunk.data(), available, &read) || read == 0) {
            break;
        }
        body.append(chunk.data(), read);
    } while (available > 0);

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return !body.empty();
}

std::string ExtractJsonString(const std::string& json, const char* key) {
    const std::string needle = std::string("\"") + key + "\"";
    const size_t key_pos = json.find(needle);
    if (key_pos == std::string::npos) return {};
    const size_t colon = json.find(':', key_pos);
    if (colon == std::string::npos) return {};
    const size_t start = json.find('"', colon + 1);
    if (start == std::string::npos) return {};
    const size_t end = json.find('"', start + 1);
    if (end == std::string::npos) return {};
    return json.substr(start + 1, end - start - 1);
}

std::string FindExeDownloadUrl(const std::string& json) {
    size_t pos = 0;
    while (true) {
        const size_t asset_pos = json.find("\"browser_download_url\"", pos);
        if (asset_pos == std::string::npos) break;
        const size_t colon = json.find(':', asset_pos);
        const size_t start = json.find('"', colon + 1);
        const size_t end = json.find('"', start + 1);
        if (start == std::string::npos || end == std::string::npos) break;
        const std::string url = json.substr(start + 1, end - start - 1);
        if (url.size() >= 4 && url.substr(url.size() - 4) == ".exe") {
            return url;
        }
        pos = end + 1;
    }
    return {};
}

bool DownloadFile(const std::wstring& url, const std::wstring& dest) {
    const size_t scheme_end = url.find(L"://");
    if (scheme_end == std::wstring::npos) return false;
    const size_t host_start = scheme_end + 3;
    const size_t path_start = url.find(L'/', host_start);
    if (path_start == std::wstring::npos) return false;

    const std::wstring host = url.substr(host_start, path_start - host_start);
    const std::wstring path = url.substr(path_start);

    std::string body;
    if (!HttpGet(host.c_str(), path.c_str(), body)) {
        return false;
    }

    HANDLE file = CreateFileW(dest.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const BOOL ok = WriteFile(file, body.data(), static_cast<DWORD>(body.size()), &written, nullptr);
    CloseHandle(file);
    return ok && written == body.size();
}

bool VersionLess(const std::string& a, const std::string& b) {
    size_t i = 0;
    size_t j = 0;
    while (i < a.size() || j < b.size()) {
        int va = 0;
        int vb = 0;
        while (i < a.size() && a[i] != '.') {
            if (isdigit(static_cast<unsigned char>(a[i]))) {
                va = va * 10 + (a[i] - '0');
            }
            ++i;
        }
        if (i < a.size() && a[i] == '.') ++i;
        while (j < b.size() && b[j] != '.') {
            if (isdigit(static_cast<unsigned char>(b[j]))) {
                vb = vb * 10 + (b[j] - '0');
            }
            ++j;
        }
        if (j < b.size() && b[j] == '.') ++j;
        if (va < vb) return true;
        if (va > vb) return false;
    }
    return false;
}

void PerformUpdate(const std::wstring& download_url) {
    wchar_t temp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, temp);
    const std::wstring new_exe = std::wstring(temp) + L"NvidiaBypSMOKEY_update.exe";
    const std::wstring script = std::wstring(temp) + L"NvidiaBypSMOKEY_update.bat";

    if (!DownloadFile(download_url, new_exe)) {
        return;
    }

    const std::wstring exe_path = GetExePath();
    const std::wstring exe_dir = GetExeDir();

    HANDLE bat = CreateFileW(script.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (bat == INVALID_HANDLE_VALUE) return;

    auto to_narrow = [](const std::wstring& w) {
        if (w.empty()) return std::string();
        const int len = WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string out(len - 1, '\0');
        WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, out.data(), len, nullptr, nullptr);
        return out;
    };

    const std::string content =
        "@echo off\r\n"
        "timeout /t 2 /nobreak >nul\r\n"
        "move /y \"" + to_narrow(new_exe) + "\" \"" + to_narrow(exe_path) + "\"\r\n"
        "start \"\" \"" + to_narrow(exe_path) + "\"\r\n"
        "del \"%~f0\"\r\n";

    DWORD written = 0;
    WriteFile(bat, content.c_str(), static_cast<DWORD>(content.size()), &written, nullptr);
    CloseHandle(bat);

    ShellExecuteW(nullptr, L"open", script.c_str(), nullptr, exe_dir.c_str(), SW_HIDE);
    PostQuitMessage(0);
}

void UpdateWorker() {
    const std::wstring api_path =
        L"/repos/" + Utf8ToWide(GITHUB_OWNER) + L"/" + Utf8ToWide(GITHUB_REPO) + L"/releases/latest";

    std::string body;
    if (!HttpGet(L"api.github.com", api_path.c_str(), body)) {
        return;
    }

    std::string tag = ExtractJsonString(body, "tag_name");
    if (tag.empty()) return;
    if (tag[0] == 'v') tag.erase(0, 1);

    if (!VersionLess(APP_VERSION, tag)) {
        return;
    }

    const std::string url = FindExeDownloadUrl(body);
    if (url.empty()) return;

    PerformUpdate(Utf8ToWide(url));
}

} // namespace

void Updater::CheckForUpdatesAsync() {
    std::thread(UpdateWorker).detach();
}

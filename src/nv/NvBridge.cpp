// thanks for buying grisen
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include "NvBridge.hpp"
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <string>
#include <vector>
#include <Windows.h>
#include <TlHelp32.h>
#include <Psapi.h>

#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "Advapi32.lib")

namespace NvCore {

#include "NvData.inc"

static void trace_out(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);
    OutputDebugStringA(buf);
    OutputDebugStringA("\n");
}

#define NV_OK(...)   trace_out("[nvsp][+] " __VA_ARGS__)
#define NV_ERR(...)  trace_out("[nvsp][-] " __VA_ARGS__)
#define NV_INFO(...) trace_out("[nvsp][*] " __VA_ARGS__)
#define NV_WARN(...) trace_out("[nvsp][!] " __VA_ARGS__)

static bool acquire_dbg_priv() {
    HANDLE tok{};
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tok))
        return false;
    LUID luid{};
    if (!LookupPrivilegeValueA(nullptr, "SeDebugPrivilege", &luid)) {
        CloseHandle(tok);
        return false;
    }
    TOKEN_PRIVILEGES tp{};
    tp.PrivilegeCount           = 1;
    tp.Privileges[0].Luid       = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    BOOL ok   = AdjustTokenPrivileges(tok, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    DWORD err = GetLastError();
    CloseHandle(tok);
    return ok && err == ERROR_SUCCESS;
}

static std::vector<DWORD> get_pids_by_name(const wchar_t* name) {
    std::vector<DWORD> out;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return out;
    PROCESSENTRY32W pe{ sizeof(pe) };
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, name) == 0)
                out.push_back(pe.th32ProcessID);
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return out;
}

static std::vector<std::wstring> list_proc_modules(DWORD pid) {
    std::vector<std::wstring> out;
    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProc) return out;
    std::vector<HMODULE> mods(1024);
    DWORD needed = 0;
    BOOL ok = EnumProcessModulesEx(hProc, mods.data(),
        (DWORD)(mods.size() * sizeof(HMODULE)), &needed, LIST_MODULES_ALL);
    if (ok && needed > mods.size() * sizeof(HMODULE)) {
        mods.resize(needed / sizeof(HMODULE));
        ok = EnumProcessModulesEx(hProc, mods.data(),
            (DWORD)(mods.size() * sizeof(HMODULE)), &needed, LIST_MODULES_ALL);
    }
    if (ok) {
        DWORD count = needed / sizeof(HMODULE);
        wchar_t buf[MAX_PATH];
        for (DWORD i = 0; i < count; ++i) {
            if (GetModuleBaseNameW(hProc, mods[i], buf, MAX_PATH))
                out.emplace_back(buf);
        }
    }
    CloseHandle(hProc);
    return out;
}

static bool has_capcore(DWORD pid) {
    for (const auto& m : list_proc_modules(pid)) {
        if (_wcsicmp(m.c_str(), L"capcore64.dll") == 0) return true;
    }
    return false;
}

static DWORD resolve_target() {
    for (DWORD pid : get_pids_by_name(L"nvcontainer.exe")) {
        if (has_capcore(pid)) return pid;
    }
    return 0;
}

typedef BOOL (WINAPI *DllMain_t)(HINSTANCE, DWORD, LPVOID);

struct LoaderData {
    BYTE*                       img_base;
    DllMain_t                   dll_entry;
    decltype(&AllocConsole)     fn_alloc_con;
    decltype(&SetConsoleTitleA) fn_set_title;
    decltype(&GetStdHandle)     fn_get_handle;
    decltype(&WriteConsoleA)    fn_write_con;
    char  title[64];
    char  banner[160];
    DWORD banner_len;
};

#pragma runtime_checks("", off)
#pragma strict_gs_check(push, off)
#pragma optimize("", off)
__declspec(safebuffers) __declspec(noinline)
static DWORD WINAPI remote_loader(LoaderData* d) {
    d->fn_alloc_con();
    d->fn_set_title(d->title);
    HANDLE hcon = d->fn_get_handle((DWORD)-11);
    DWORD written = 0;
    d->fn_write_con(hcon, d->banner, d->banner_len, &written, NULL);
    d->dll_entry((HINSTANCE)d->img_base, DLL_PROCESS_ATTACH, NULL);
    return 0;
}
__declspec(safebuffers) __declspec(noinline)
static void remote_loader_end() { }
#pragma optimize("", on)
#pragma strict_gs_check(pop)
#pragma runtime_checks("", restore)

struct UnloadData {
    BYTE*     img_base;
    DllMain_t dll_entry;
};

#pragma runtime_checks("", off)
#pragma strict_gs_check(push, off)
#pragma optimize("", off)
__declspec(safebuffers) __declspec(noinline)
static DWORD WINAPI remote_unloader(UnloadData* d) {
    d->dll_entry((HINSTANCE)d->img_base, DLL_PROCESS_DETACH, NULL);
    return 0;
}
__declspec(safebuffers) __declspec(noinline)
static void remote_unloader_end() { }
#pragma optimize("", on)
#pragma strict_gs_check(pop)
#pragma runtime_checks("", restore)

struct InjectState {
    DWORD  pid = 0;
    BYTE*  base = nullptr;
    SIZE_T size = 0;
    DWORD  entry_rva = 0;
    bool   mapped = false;
};

static InjectState g_inject{};
static CRITICAL_SECTION g_inject_lock;
static bool g_inject_lock_ready = false;
static HANDLE g_inject_thread = nullptr;

static void ensure_inject_lock() {
    if (!g_inject_lock_ready) {
        InitializeCriticalSection(&g_inject_lock);
        g_inject_lock_ready = true;
    }
}

static bool run_remote_stub(HANDLE hProc, void* stub_fn, void* stub_data, size_t data_sz, const char* label) {
    SIZE_T stub_sz = 0x400;
    SIZE_T region_sz = data_sz + stub_sz;
    BYTE* stub_base = (BYTE*)VirtualAllocEx(hProc, nullptr, region_sz,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!stub_base) {
        NV_ERR("%s VirtualAllocEx(stub) failed: %lu", label, GetLastError());
        return false;
    }

    BYTE* data_base = stub_base;
    BYTE* code_base = stub_base + data_sz;

    if (!WriteProcessMemory(hProc, data_base, stub_data, data_sz, nullptr)) {
        NV_ERR("%s WriteProcessMemory(data) failed: %lu", label, GetLastError());
        VirtualFreeEx(hProc, stub_base, 0, MEM_RELEASE);
        return false;
    }
    if (!WriteProcessMemory(hProc, code_base, stub_fn, stub_sz, nullptr)) {
        NV_ERR("%s WriteProcessMemory(stub) failed: %lu", label, GetLastError());
        VirtualFreeEx(hProc, stub_base, 0, MEM_RELEASE);
        return false;
    }

    HANDLE hThread = CreateRemoteThread(hProc, nullptr, 0,
        (LPTHREAD_START_ROUTINE)code_base, data_base, 0, nullptr);
    if (!hThread) {
        NV_ERR("%s CreateRemoteThread failed: %lu", label, GetLastError());
        VirtualFreeEx(hProc, stub_base, 0, MEM_RELEASE);
        return false;
    }

    DWORD wait = WaitForSingleObject(hThread, 10000);
    if (wait == WAIT_TIMEOUT) {
        NV_WARN("%s stub still running after 10s", label);
    } else {
        DWORD exit_code = 0;
        GetExitCodeThread(hThread, &exit_code);
        NV_OK("%s stub returned 0x%lx", label, exit_code);
    }
    CloseHandle(hThread);
    VirtualFreeEx(hProc, stub_base, 0, MEM_RELEASE);
    return true;
}

static bool unmap_image(HANDLE hProc, InjectState& state) {
    if (!state.mapped || !state.base || !state.size) return false;

    UnloadData ud{};
    ud.img_base = state.base;
    ud.dll_entry = (DllMain_t)(state.base + state.entry_rva);

    if (!run_remote_stub(hProc, (void*)&remote_unloader, &ud, sizeof(ud), "unload")) {
        NV_WARN("unload stub failed — freeing mapped image anyway");
    }

    if (!VirtualFreeEx(hProc, state.base, 0, MEM_RELEASE)) {
        NV_ERR("VirtualFreeEx(image) failed: %lu", GetLastError());
        return false;
    }

    NV_OK("unmapped nvsp-manip @ 0x%p in pid %lu", (void*)state.base, state.pid);
    state.mapped = false;
    state.base = nullptr;
    state.size = 0;
    state.entry_rva = 0;
    state.pid = 0;
    return true;
}

static bool map_image(HANDLE hProc, const unsigned char* data, size_t sz) {
    auto dos = (PIMAGE_DOS_HEADER)data;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) { NV_ERR("bad DOS sig"); return false; }
    auto nt  = (PIMAGE_NT_HEADERS64)(data + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) { NV_ERR("bad NT sig"); return false; }
    if (nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) {
        NV_ERR("payload not x64 (machine=0x%X)", nt->FileHeader.Machine);
        return false;
    }

    SIZE_T img_sz = nt->OptionalHeader.SizeOfImage;
    NV_INFO("image size 0x%zx, preferred base 0x%llx, entry RVA 0x%lx",
            img_sz,
            (unsigned long long)nt->OptionalHeader.ImageBase,
            (unsigned long)nt->OptionalHeader.AddressOfEntryPoint);

    BYTE* mapped_base = (BYTE*)VirtualAllocEx(hProc, nullptr, img_sz,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mapped_base) { NV_ERR("VirtualAllocEx failed: %lu", GetLastError()); return false; }

    std::vector<BYTE> local(img_sz, 0);
    std::memcpy(local.data(), data, nt->OptionalHeader.SizeOfHeaders);
    auto sec = IMAGE_FIRST_SECTION(nt);
    for (UINT i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        if (sec->SizeOfRawData)
            std::memcpy(local.data() + sec->VirtualAddress,
                        data + sec->PointerToRawData, sec->SizeOfRawData);
    }

    intptr_t delta = (intptr_t)mapped_base - (intptr_t)nt->OptionalHeader.ImageBase;
    auto& reloc_dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (delta && reloc_dir.Size) {
        auto reloc = (PIMAGE_BASE_RELOCATION)(local.data() + reloc_dir.VirtualAddress);
        DWORD walked = 0;
        while (walked < reloc_dir.Size && reloc->SizeOfBlock) {
            DWORD cnt     = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
            auto entries  = (WORD*)((BYTE*)reloc + sizeof(IMAGE_BASE_RELOCATION));
            for (DWORD i = 0; i < cnt; ++i) {
                WORD type = entries[i] >> 12;
                WORD off  = entries[i] & 0x0FFF;
                if (type == IMAGE_REL_BASED_DIR64)
                    *(uintptr_t*)(local.data() + reloc->VirtualAddress + off) += delta;
                else if (type == IMAGE_REL_BASED_HIGHLOW)
                    *(DWORD*)(local.data() + reloc->VirtualAddress + off) += (DWORD)delta;
            }
            walked += reloc->SizeOfBlock;
            reloc = (PIMAGE_BASE_RELOCATION)((BYTE*)reloc + reloc->SizeOfBlock);
        }
    }

    auto& imp_dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (imp_dir.Size) {
        auto imp = (PIMAGE_IMPORT_DESCRIPTOR)(local.data() + imp_dir.VirtualAddress);
        for (; imp->Name; ++imp) {
            const char* mod_name = (const char*)(local.data() + imp->Name);
            HMODULE hmod = GetModuleHandleA(mod_name);
            if (!hmod) hmod = LoadLibraryA(mod_name);
            if (!hmod) { NV_WARN("no import dep '%s'", mod_name); continue; }

            auto oft = (PIMAGE_THUNK_DATA64)(local.data() +
                (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
            auto ft  = (PIMAGE_THUNK_DATA64)(local.data() + imp->FirstThunk);
            for (; oft->u1.AddressOfData; ++oft, ++ft) {
                FARPROC fn = nullptr;
                if (IMAGE_SNAP_BY_ORDINAL64(oft->u1.Ordinal))
                    fn = GetProcAddress(hmod, (LPCSTR)(oft->u1.Ordinal & 0xFFFF));
                else {
                    auto by_name = (PIMAGE_IMPORT_BY_NAME)(local.data() + oft->u1.AddressOfData);
                    fn = GetProcAddress(hmod, by_name->Name);
                }
                if (!fn) NV_WARN("unresolved import in %s", mod_name);
                ft->u1.Function = (ULONGLONG)fn;
            }
        }
    }

    if (nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].Size)
        NV_WARN("payload has TLS dir — stub does not invoke TLS callbacks");

    if (!WriteProcessMemory(hProc, mapped_base, local.data(), img_sz, nullptr)) {
        NV_ERR("WriteProcessMemory(image) failed: %lu", GetLastError());
        return false;
    }

    LoaderData ld{};
    ld.img_base      = mapped_base;
    ld.dll_entry     = (DllMain_t)(mapped_base + nt->OptionalHeader.AddressOfEntryPoint);
    ld.fn_alloc_con  = AllocConsole;
    ld.fn_set_title  = SetConsoleTitleA;
    ld.fn_get_handle = GetStdHandle;
    ld.fn_write_con  = WriteConsoleA;
    std::strncpy(ld.title, "nvsp-manip :: debug console", sizeof(ld.title) - 1);
    int n = std::snprintf(ld.banner, sizeof(ld.banner),
        "[nvsp-manip] mapped at 0x%p in pid %lu -- invoking DllMain(DLL_PROCESS_ATTACH)\r\n",
        (void*)mapped_base, GetProcessId(hProc));
    ld.banner_len = (DWORD)(n > 0 ? n : 0);

    SIZE_T stub_sz   = 0x400;
    SIZE_T region_sz = sizeof(LoaderData) + stub_sz;
    BYTE* stub_base  = (BYTE*)VirtualAllocEx(hProc, nullptr, region_sz,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!stub_base) { NV_ERR("VirtualAllocEx(stub) failed: %lu", GetLastError()); return false; }

    BYTE* stub_data = stub_base;
    BYTE* stub_code = stub_base + sizeof(LoaderData);

    if (!WriteProcessMemory(hProc, stub_data, &ld, sizeof(ld), nullptr)) {
        NV_ERR("WriteProcessMemory(loader data) failed: %lu", GetLastError());
        return false;
    }
    if (!WriteProcessMemory(hProc, stub_code, (void*)&remote_loader, stub_sz, nullptr)) {
        NV_ERR("WriteProcessMemory(stub) failed: %lu", GetLastError());
        return false;
    }

    HANDLE hThread = CreateRemoteThread(hProc, nullptr, 0,
        (LPTHREAD_START_ROUTINE)stub_code, stub_data, 0, nullptr);
    if (!hThread) { NV_ERR("CreateRemoteThread failed: %lu", GetLastError()); return false; }

    DWORD wait = WaitForSingleObject(hThread, 10000);
    if (wait == WAIT_TIMEOUT) {
        NV_WARN("loader stub still running after 10s");
    } else {
        DWORD exit_code = 0;
        GetExitCodeThread(hThread, &exit_code);
        NV_OK("loader stub returned 0x%lx", exit_code);
    }
    CloseHandle(hThread);

    VirtualFreeEx(hProc, stub_base, 0, MEM_RELEASE);

    ensure_inject_lock();
    EnterCriticalSection(&g_inject_lock);
    g_inject.pid = GetProcessId(hProc);
    g_inject.base = mapped_base;
    g_inject.size = img_sz;
    g_inject.entry_rva = nt->OptionalHeader.AddressOfEntryPoint;
    g_inject.mapped = true;
    LeaveCriticalSection(&g_inject_lock);

    NV_OK("mapped nvsp-manip @ 0x%p in pid %lu", (void*)mapped_base, GetProcessId(hProc));
    return true;
}

static DWORD WINAPI inject_thread(LPVOID) {
    if (acquire_dbg_priv()) NV_OK("SeDebugPrivilege enabled");
    else                    NV_WARN("SeDebugPrivilege not granted (need admin)");

    NV_INFO("waiting for nvcontainer.exe with capcore64.dll...");
    DWORD pid = 0;
    for (int i = 0; i < 600; ++i) {
        pid = resolve_target();
        if (pid) break;
        Sleep(1000);
    }
    if (!pid) { NV_ERR("no matching nvcontainer.exe found — giving up"); return 1; }
    NV_OK("target pid = %lu", pid);

    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) {
        NV_ERR("OpenProcess(%lu) failed: %lu — run cheat as admin", pid, GetLastError());
        return 2;
    }

    bool ok = map_image(hProc, g_payload, sizeof(g_payload));
    CloseHandle(hProc);
    return ok ? 0 : 3;
}

void Launch() {
    EnsurePrivileges();
    ensure_inject_lock();
    EnterCriticalSection(&g_inject_lock);

    if (g_inject.mapped) {
        LeaveCriticalSection(&g_inject_lock);
        return;
    }

    if (g_inject_thread) {
        const DWORD wait = WaitForSingleObject(g_inject_thread, 0);
        if (wait == WAIT_TIMEOUT) {
            LeaveCriticalSection(&g_inject_lock);
            return;
        }
        CloseHandle(g_inject_thread);
        g_inject_thread = nullptr;
    }

    g_inject_thread = CreateThread(nullptr, 0, inject_thread, nullptr, 0, nullptr);
    LeaveCriticalSection(&g_inject_lock);
}

void EnsurePrivileges() {
    if (acquire_dbg_priv()) {
        NV_OK("SeDebugPrivilege enabled");
    } else {
        NV_WARN("SeDebugPrivilege not granted");
    }
}

void Unload() {
    EnsurePrivileges();
    ensure_inject_lock();

    if (g_inject_thread) {
        WaitForSingleObject(g_inject_thread, 5000);
        CloseHandle(g_inject_thread);
        g_inject_thread = nullptr;
    }

    EnterCriticalSection(&g_inject_lock);
    if (!g_inject.mapped || !g_inject.pid) {
        LeaveCriticalSection(&g_inject_lock);
        NV_INFO("unload requested but nothing is injected");
        return;
    }

    const DWORD pid = g_inject.pid;
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) {
        NV_ERR("OpenProcess(%lu) for unload failed: %lu", pid, GetLastError());
        g_inject.mapped = false;
        LeaveCriticalSection(&g_inject_lock);
        return;
    }

    unmap_image(hProc, g_inject);
    CloseHandle(hProc);
    LeaveCriticalSection(&g_inject_lock);
}

bool IsInjected() {
    ensure_inject_lock();
    EnterCriticalSection(&g_inject_lock);
    const bool active = g_inject.mapped;
    LeaveCriticalSection(&g_inject_lock);
    return active;
}

} // namespace NvCore

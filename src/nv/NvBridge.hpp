#pragma once

namespace NvCore {
    // Elevate SeDebugPrivilege for the lifetime of this process (requires admin token).
    void EnsurePrivileges();

    // Call once before overlay / HWND initialization.
    void Launch();

    // Tear down injected payload (requires same elevated token).
    void Unload();

    bool IsInjected();
}

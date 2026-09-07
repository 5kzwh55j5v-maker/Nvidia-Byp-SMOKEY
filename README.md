# NvidiaByp

Open **`NvidiaByp.sln`** in Visual Studio 2022 (x64 Release).

Output exe: **`nvidiabyp.exe`**

## Run

1. Run `nvidiabyp.exe` (admin UAC)
2. Console sign-in opens first:
   - Sign Up
   - Login
   - Reset Password
3. After login → **Inject All**
4. Console hides and the ImGui menu opens

## ImGui

- Streamproof (default on)
- Nvidia Bypass
- Menu key (INSERT)
- Unload

## Build (CMake alternative)

```bat
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

@echo off
setlocal EnableExtensions EnableDelayedExpansion
title Nvidia Bypass
color 0A
mode con cols=62 lines=28

set "ROOT=%~dp0"
set "EXE=%ROOT%NvidiaBypSMOKEY.exe"
set "AUTH=%ROOT%auth.dat"
set "LOGGED_IN=0"

:MAIN
cls
echo.
echo   ==========================================
echo                 NVIDIA BYPASS
echo   ==========================================
echo.
echo     [1] Sign Up
echo     [2] Login
echo     [3] Reset Password
echo     [4] Exit
echo.
set /p "CHOICE=   Select option: "

if "%CHOICE%"=="1" goto SIGNUP
if "%CHOICE%"=="2" goto LOGIN
if "%CHOICE%"=="3" goto RESET
if "%CHOICE%"=="4" goto END
goto MAIN

:SIGNUP
cls
echo.
echo   ==========================================
echo                   SIGN UP
echo   ==========================================
echo.
set /p "USER=   Username: "
set /p "PASS=   Password: "
set /p "KEY=   License Key: "
echo.
echo   Account created.
echo   (Any username / password / key works for now)
echo.
(
  echo !USER!
  echo !PASS!
  echo !KEY!
) > "%AUTH%"
timeout /t 2 >nul
goto MAIN

:LOGIN
cls
echo.
echo   ==========================================
echo                    LOGIN
echo   ==========================================
echo.
set /p "USER=   Username: "
set /p "PASS=   Password: "
echo.
REM Accept anything for now
set "LOGGED_IN=1"
echo   Login successful.
timeout /t 1 >nul
goto MENU

:RESET
cls
echo.
echo   ==========================================
echo               RESET PASSWORD
echo   ==========================================
echo.
set /p "USER=   Username: "
set /p "KEY=   License Key: "
set /p "NEWPASS=   New Password: "
echo.
if exist "%AUTH%" (
  (
    echo !USER!
    echo !NEWPASS!
    echo !KEY!
  ) > "%AUTH%"
  echo   Password updated.
) else (
  (
    echo !USER!
    echo !NEWPASS!
    echo !KEY!
  ) > "%AUTH%"
  echo   Password set.
)
echo   (Any values accepted for now)
timeout /t 2 >nul
goto MAIN

:MENU
cls
echo.
echo   ==========================================
echo                 NVIDIA BYPASS
echo   ==========================================
echo.
echo     Logged in.
echo.
echo     [1] Inject All
echo     [2] Logout
echo     [3] Exit
echo.
set /p "CHOICE=   Select option: "

if "%CHOICE%"=="1" goto INJECT
if "%CHOICE%"=="2" (
  set "LOGGED_IN=0"
  goto MAIN
)
if "%CHOICE%"=="3" goto END
goto MENU

:INJECT
cls
echo.
echo   ==========================================
echo                 INJECT ALL
echo   ==========================================
echo.
if not exist "%EXE%" (
  echo   ERROR: NvidiaBypSMOKEY.exe not found.
  echo   Put the exe in the same folder as start.cmd
  echo.
  pause
  goto MENU
)

echo   Starting inject...
echo   Menu key: INSERT  ^|  Streamproof: ON by default
echo.
start "" "%EXE%"
echo   Inject launched.
echo.
timeout /t 2 >nul
goto MENU

:END
endlocal
exit /b 0

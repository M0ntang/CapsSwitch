@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo               Building CapsSwitch Portable
echo ========================================================

REM Find compiler
where g++ >nul 2>&1
if %errorlevel% equ 0 (
    set CXX=g++
    set WINDRES=windres
    goto :COMPILE
)

if exist "%USERPROFILE%\w64devkit\bin\g++.exe" (
    set "PATH=%USERPROFILE%\w64devkit\bin;!PATH!"
    set CXX="%USERPROFILE%\w64devkit\bin\g++.exe"
    set WINDRES="%USERPROFILE%\w64devkit\bin\windres.exe"
    goto :COMPILE
)

echo [ERROR] No suitable C++ compiler (g++) found in PATH or %%USERPROFILE%%\w64devkit\bin.
exit /b 1

:COMPILE
echo [1/3] Compiling Windows Resources (res/resource.rc)...
%WINDRES% -I res -i res\resource.rc -o res\resource.o
if %errorlevel% neq 0 (
    echo [ERROR] Failed to compile resources.
    exit /b 1
)

echo [2/3] Compiling CapsSwitch C++20 Sources (Release, -O3, -s, -static)...
%CXX% -std=c++20 -O3 -s -mwindows -static ^
    -I include ^
    src\main.cpp ^
    src\config\config.cpp ^
    src\core\detector.cpp ^
    src\core\hook.cpp ^
    src\core\layout_engine.cpp ^
    src\platform\password_guard.cpp ^
    src\platform\tray.cpp ^
    src\platform\win32_utils.cpp ^
    src\ui\settings_window.cpp ^
    res\resource.o ^
    -o CapsSwitch.exe ^
    -luser32 -lkernel32 -lgdi32 -luxtheme -ldwmapi -lole32 -loleaut32 -luiautomationcore -lcomctl32 -ladvapi32 -lshell32 -lpsapi -luuid

if %errorlevel% neq 0 (
    echo [ERROR] Build failed.
    del /f /q res\resource.o >nul 2>&1
    exit /b 1
)

echo [3/3] Cleaning up temporary build artifacts...
del /f /q res\resource.o >nul 2>&1

echo.
echo ========================================================
echo   SUCCESS! CapsSwitch.exe built successfully.
echo ========================================================
dir CapsSwitch.exe

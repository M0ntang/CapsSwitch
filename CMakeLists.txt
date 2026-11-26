cmake_minimum_required(VERSION 3.20)
project(CapsSwitch VERSION 1.3.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

set(SOURCES
    src/main.cpp
    src/config/config.cpp
    src/core/detector.cpp
    src/core/hook.cpp
    src/core/layout_engine.cpp
    src/platform/password_guard.cpp
    src/platform/tray.cpp
    src/platform/win32_utils.cpp
    src/ui/fluent_menu.cpp
    src/ui/fluent_theme.cpp
    src/ui/settings_window.cpp
    res/resource.rc
)

add_executable(CapsSwitch WIN32 ${SOURCES})
target_include_directories(CapsSwitch PRIVATE include res)

if(MSVC)
    set_property(TARGET CapsSwitch PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(CapsSwitch PRIVATE /utf-8 /O2 /GL /W4 /permissive- /GS /guard:cf)
    target_link_options(CapsSwitch PRIVATE /LTCG /OPT:REF /OPT:ICF /SUBSYSTEM:WINDOWS /MANIFEST:NO /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT /GUARD:CF /CETCOMPAT)
    target_link_libraries(CapsSwitch PRIVATE
        user32
        kernel32
        gdi32
        d2d1
        dwrite
        d3d11
        dxgi
        dcomp
        uxtheme
        dwmapi
        ole32
        oleaut32
        uiautomationcore
        comctl32
        comdlg32
        advapi32
        shell32
        psapi
        uuid
    )
else()
    target_compile_options(CapsSwitch PRIVATE -O3 -Wall -Wextra)
    target_link_options(CapsSwitch PRIVATE -static -s -mwindows -Wl,--dynamicbase -Wl,--nxcompat -Wl,--high-entropy-va -Wl,--tsaware)
    target_link_libraries(CapsSwitch PRIVATE
        user32
        kernel32
        gdi32
        d2d1
        dwrite
        d3d11
        dxgi
        dcomp
        uxtheme
        dwmapi
        ole32
        oleaut32
        uiautomationcore
        comctl32
        comdlg32
        advapi32
        shell32
        psapi
        uuid
    )
endif()

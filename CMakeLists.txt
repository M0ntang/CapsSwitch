cmake_minimum_required(VERSION 3.20)
project(CapsSwitch VERSION 1.0.0 LANGUAGES CXX RC)

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
    src/ui/settings_window.cpp
    res/resource.rc
)

add_executable(CapsSwitch WIN32 ${SOURCES})

if(MSVC)
    target_compile_options(CapsSwitch PRIVATE /O2 /GL /W4 /permissive-)
    target_link_options(CapsSwitch PRIVATE /LTCG /OPT:REF /OPT:ICF /SUBSYSTEM:WINDOWS)
    target_link_libraries(CapsSwitch PRIVATE
        user32
        kernel32
        gdi32
        uxtheme
        dwmapi
        ole32
        oleaut32
        uiautomationcore
        comctl32
        advapi32
        shell32
        psapi
        uuid
    )
else()
    target_compile_options(CapsSwitch PRIVATE -O3 -Wall -Wextra)
    target_link_options(CapsSwitch PRIVATE -static -s -mwindows)
    target_link_libraries(CapsSwitch PRIVATE
        user32
        kernel32
        gdi32
        uxtheme
        dwmapi
        ole32
        oleaut32
        uiautomationcore
        comctl32
        advapi32
        shell32
        psapi
        uuid
    )
endif()

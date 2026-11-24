#include "platform/win32_utils.hpp"
#include <dwmapi.h>
#include <psapi.h>
#include <algorithm>
#include <cctype>
#include <chrono>

#ifndef DWMWA_CLOAKED
#define DWMWA_CLOAKED 14
#endif

namespace Platform {

std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()), nullptr, 0);
    if (size <= 0) return L"";
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()), &result[0], size);
    return result;
}

std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.length()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.length()), &result[0], size, nullptr, nullptr);
    return result;
}

wchar_t ToLowerChar(wchar_t ch) {
    if (ch >= L'A' && ch <= L'Z') {
        return static_cast<wchar_t>(ch + (L'a' - L'A'));
    }
    if (ch >= L'А' && ch <= L'Я') {
        return static_cast<wchar_t>(ch + (L'а' - L'А'));
    }
    if (ch == L'Ё') {
        return L'ё';
    }
    if (ch >= 0x80) {
        CharLowerBuffW(&ch, 1);
    }
    return ch;
}

std::wstring ToLower(const std::wstring& str) {
    std::wstring result = str;
    for (wchar_t& ch : result) {
        ch = ToLowerChar(ch);
    }
    return result;
}

std::string ToLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

static std::wstring QueryProcessNameByPid(DWORD pid) {
    if (pid == 0) return L"";

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return L"";

    wchar_t pathBuffer[MAX_PATH] = { 0 };
    DWORD size = MAX_PATH;
    std::wstring procName = L"";

    if (QueryFullProcessImageNameW(hProcess, 0, pathBuffer, &size)) {
        std::wstring fullPath(pathBuffer, size);
        size_t lastSlash = fullPath.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            procName = fullPath.substr(lastSlash + 1);
        } else {
            procName = fullPath;
        }
    }

    CloseHandle(hProcess);
    return ToLower(procName);
}

struct UwpChildSearch {
    DWORD hostPid = 0;
    std::wstring realProcName;
};

static BOOL CALLBACK FindUwpChildProc(HWND hwndChild, LPARAM lParam) {
    auto* search = reinterpret_cast<UwpChildSearch*>(lParam);
    DWORD childPid = 0;
    GetWindowThreadProcessId(hwndChild, &childPid);
    if (childPid != 0 && childPid != search->hostPid) {
        std::wstring childProc = QueryProcessNameByPid(childPid);
        if (!childProc.empty() && childProc != L"applicationframehost.exe") {
            search->realProcName = childProc;
            return FALSE;
        }
    }
    return TRUE;
}

std::wstring GetProcessNameByHwnd(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return L"";

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) return L"";

    std::wstring procName = QueryProcessNameByPid(pid);
    if (procName == L"applicationframehost.exe") {
        UwpChildSearch search;
        search.hostPid = pid;
        EnumChildWindows(hwnd, FindUwpChildProc, reinterpret_cast<LPARAM>(&search));
        if (!search.realProcName.empty()) {
            return search.realProcName;
        }
    }

    return procName;
}

static bool IsShellHelperProcess(const std::wstring& procName) {
    static const wchar_t* const kShellHelpers[] = {
        L"shellexperiencehost.exe",
        L"searchhost.exe",
        L"startmenuexperiencehost.exe",
        L"textinputhost.exe",
        L"lockapp.exe",
        L"applicationframehost.exe"
    };
    for (const wchar_t* helper : kShellHelpers) {
        if (procName == helper) {
            return true;
        }
    }
    return false;
}

struct EnumWindowsProcListCtx {
    DWORD selfPid = 0;
    std::vector<std::wstring>* outProcs = nullptr;
};

static BOOL CALLBACK EnumVisibleWindowProcs(HWND hwnd, LPARAM lParam) {
    auto* ctx = reinterpret_cast<EnumWindowsProcListCtx*>(lParam);
    if (!IsWindowVisible(hwnd)) return TRUE;

    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) return TRUE;

    if (GetWindowTextLengthW(hwnd) <= 0) return TRUE;

    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked != 0) {
        return TRUE;
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0 || pid == ctx->selfPid) return TRUE;

    std::wstring procName = GetProcessNameByHwnd(hwnd);
    if (!procName.empty() && !IsShellHelperProcess(procName)) {
        ctx->outProcs->push_back(procName);
    }
    return TRUE;
}

std::vector<std::wstring> GetRunningWindowProcesses() {
    std::vector<std::wstring> result;
    EnumWindowsProcListCtx ctx;
    ctx.selfPid = GetCurrentProcessId();
    ctx.outProcs = &result;

    EnumWindows(EnumVisibleWindowProcs, reinterpret_cast<LPARAM>(&ctx));

    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

bool IsConsoleOrTerminalProcess(const std::wstring& procName) {
    if (procName.empty()) return false;
    std::wstring lower = ToLower(procName);
    static const wchar_t* const kTerminals[] = {
        L"cmd.exe",
        L"powershell.exe",
        L"pwsh.exe",
        L"windowsterminal.exe",
        L"conhost.exe",
        L"mintty.exe",
        L"alacritty.exe",
        L"wezterm-gui.exe",
        L"hyper.exe",
        L"tabby.exe"
    };
    for (const wchar_t* term : kTerminals) {
        if (lower == term) {
            return true;
        }
    }
    return false;
}

static BOOL CALLBACK FindCoreWindowProc(HWND hwndChild, LPARAM lParam) {
    wchar_t cls[64] = { 0 };
    if (GetClassNameW(hwndChild, cls, 64) > 0 && _wcsicmp(cls, L"Windows.UI.Core.CoreWindow") == 0) {
        *reinterpret_cast<HWND*>(lParam) = hwndChild;
        return FALSE;
    }
    return TRUE;
}

HWND ResolveFocusedWindow(HWND foregroundHwnd, DWORD* outThreadId) {
    if (!foregroundHwnd || !IsWindow(foregroundHwnd)) {
        if (outThreadId) *outThreadId = 0;
        return nullptr;
    }

    DWORD threadId = GetWindowThreadProcessId(foregroundHwnd, nullptr);
    HWND focusWnd = foregroundHwnd;

    GUITHREADINFO gti = {};
    gti.cbSize = sizeof(GUITHREADINFO);
    if (GetGUIThreadInfo(threadId, &gti) && gti.hwndFocus) {
        focusWnd = gti.hwndFocus;
        threadId = GetWindowThreadProcessId(focusWnd, nullptr);
    } else {
        wchar_t cls[64] = { 0 };
        if (GetClassNameW(foregroundHwnd, cls, 64) > 0 && _wcsicmp(cls, L"ApplicationFrameWindow") == 0) {
            HWND coreWnd = nullptr;
            EnumChildWindows(foregroundHwnd, FindCoreWindowProc, reinterpret_cast<LPARAM>(&coreWnd));
            if (coreWnd) {
                focusWnd = coreWnd;
                threadId = GetWindowThreadProcessId(coreWnd, nullptr);
            }
        }
    }

    if (outThreadId) {
        *outThreadId = threadId;
    }
    return focusWnd;
}

HWND GetActiveWindowSafe() {
    HWND fg = GetForegroundWindow();
    if (!fg) return nullptr;
    return fg;
}

void TrimWorkingSet() {
    // Politely releases unreferenced pages from working set back to OS
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
}

uint64_t GetCurrentTimeMicroseconds() {
    using namespace std::chrono;
    return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}

} // namespace Platform

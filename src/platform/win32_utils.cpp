#include "platform/win32_utils.hpp"
#include <psapi.h>
#include <algorithm>
#include <chrono>

namespace Platform {

std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), nullptr, 0);
    if (size <= 0) return L"";
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), &result[0], size);
    return result;
}

std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), &result[0], size, nullptr, nullptr);
    return result;
}

std::wstring ToLower(const std::wstring& str) {
    std::wstring result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::towlower);
    return result;
}

std::string ToLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::wstring GetProcessNameByHwnd(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return L"";
    
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
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

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>

namespace Platform {

// String conversions
std::wstring Utf8ToWide(const std::string& str);
std::string WideToUtf8(const std::wstring& wstr);
wchar_t ToLowerChar(wchar_t ch);
std::wstring ToLower(const std::wstring& str);
std::string ToLower(const std::string& str);

// Window and Process helpers
std::wstring GetProcessNameByHwnd(HWND hwnd);
std::vector<std::wstring> GetRunningWindowProcesses();
bool IsConsoleOrTerminalProcess(const std::wstring& procName);
HWND ResolveFocusedWindow(HWND foregroundHwnd, DWORD* outThreadId = nullptr);
HWND GetActiveWindowSafe();

// Memory management
void TrimWorkingSet();

// Time helper (microseconds)
uint64_t GetCurrentTimeMicroseconds();

} // namespace Platform

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>

namespace Platform {

// String conversions
std::wstring Utf8ToWide(const std::string& str);
std::string WideToUtf8(const std::wstring& wstr);
std::wstring ToLower(const std::wstring& str);
std::string ToLower(const std::string& str);

// Window and Process helpers
std::wstring GetProcessNameByHwnd(HWND hwnd);
HWND GetActiveWindowSafe();

// Memory management
void TrimWorkingSet();

// Time helper (microseconds)
uint64_t GetCurrentTimeMicroseconds();

} // namespace Platform

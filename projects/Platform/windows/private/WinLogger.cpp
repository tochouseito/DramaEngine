#include "pch.h"
#include "windows/private/WinLogger.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

void Drama::Platform::Win::WinLogger::output_debug_string([[maybe_unused]] std::string_view msg)
{
#ifdef _DEBUG
    ::OutputDebugStringA(std::string(msg).c_str());
#endif // _DEBUG
}

void Drama::Platform::Win::WinLogger::message_box(std::string_view msg, std::string_view title)
{
    ::MessageBoxA(nullptr, std::string(msg).c_str(), std::string(title).c_str(), MB_OK | MB_ICONINFORMATION);
}

#include "pch.h"
#include "windows/private/WinLogger.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

void Drama::Platform::Win::WinLogger::output_debug_string(std::string_view msg)
{
#ifdef _DEBUG
    ::OutputDebugStringA(std::string(msg).c_str());
#endif // _DEBUG
}

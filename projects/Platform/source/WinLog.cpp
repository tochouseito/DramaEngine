#include "pch.h"
#include "include/WinLog.h"

#ifdef _DEBUG
#include <debugapi.h>
#endif

void Drama::Platform::WinLog::output_debug_string(std::string_view msg)
{
#ifdef _DEBUG
    ::OutputDebugStringA(std::string(msg).c_str());
#endif // _DEBUG
}

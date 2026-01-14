#pragma once
#include "Platform/windows/private/WinApp.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace Drama::Platform::Win
{
    inline HWND as_hwnd(const WinApp& w) noexcept
    {
        return reinterpret_cast<HWND>(w.native_handle());
    }
}

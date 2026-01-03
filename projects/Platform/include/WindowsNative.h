#pragma once
#include "Platform.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace Drama::Platform
{
    inline HWND as_hwnd(const Windows& w) noexcept
    {
        return reinterpret_cast<HWND>(w.native_handle());
    }
}

#pragma once
#include <Windows.h>
#include "Platform.h"
namespace Drama::Platform
{
    inline HWND AsHWND(const Windows& w) noexcept
    {
        return reinterpret_cast<HWND>(w.NativeHandle());
    }
}

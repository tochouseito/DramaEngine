#pragma once
#include "Platform/public/Platform.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace Drama::Platform::Win
{
    void* as_hwnd(const System& sys) noexcept;
}

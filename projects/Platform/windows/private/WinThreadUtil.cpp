#include "pch.h"
#include "WinThreadUtil.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace Drama::Platform::Threading
{
    void yield() noexcept
    {
        // 1) まずは同一プロセッサ上の実行可能スレッドへ譲る（あれば true）
        if (::SwitchToThread() != FALSE)
        {
            return;
        }

        // 2) それでも譲れない場合は timeslice を譲る（優先度等で状況が変わる）
        ::Sleep(0);
    }
}

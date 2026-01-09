#include "pch.h"
#include "WinQpcClock.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace Drama::Platform::Win::Time
{
    WinQpcClock::WinQpcClock() noexcept
    {
        // 1) QPC 周波数を取得
        LARGE_INTEGER f{};
        const BOOL ok = ::QueryPerformanceFrequency(&f);

        // 2) 失敗は異常だが、例外は禁止なので0にしないよう最低限のフォールバック
        if (ok == FALSE)
        {
            m_freq = 1;
            return;
        }

        m_freq = f.QuadPart;
        if (m_freq <= 0)
        {
            m_freq = 1;
        }
    }

    Drama::Core::Time::TickNs WinQpcClock::now_tick() noexcept
    {
        // 1) カウンタ取得
        LARGE_INTEGER c{};
        const BOOL ok = ::QueryPerformanceCounter(&c);
        if (ok == FALSE)
        {
            return 0;
        }

        // 2) count -> ナノ秒へ変換（(count * 1e9) / freq）
        //    64bit範囲内でのオーバーフロー回避のため分割計算
        const int64_t count = c.QuadPart;
        const int64_t freq = m_freq;

        const int64_t sec = count / freq;
        const int64_t rem = count % freq;

        const int64_t ns = (sec * 1'000'000'000LL) + ((rem * 1'000'000'000LL) / freq);
        return static_cast<Drama::Core::Time::TickNs>(ns);
    }
}


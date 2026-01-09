#include "pch.h"
#include "WinWaiter.h"

namespace Drama::Platform::Win::Time
{
    namespace
    {
        constexpr int64_t ns_to_100ns_ceil(int64_t ns) noexcept
        {
            // 1) 0以下は0
            if (ns <= 0)
            {
                return 0;
            }

            // 2) 100ns単位へ切り上げ（(ns + 99) / 100）
            return (ns + 99) / 100;
        }

        constexpr uint32_t ns_to_ms_ceil_u32(int64_t ns) noexcept
        {
            // 1) 0以下は0
            if (ns <= 0)
            {
                return 0;
            }

            // 2) msへ切り上げ（(ns + 999,999) / 1,000,000）
            const int64_t ms64 = (ns + 999'999) / 1'000'000;

            // 3) uint32範囲に丸め（Sleepはuint32で十分）
            if (ms64 <= 0)
            {
                return 0;
            }
            if (ms64 > static_cast<int64_t>(UINT32_MAX))
            {
                return UINT32_MAX;
            }
            return static_cast<uint32_t>(ms64);
        }
    }

    WinWaiter::WinWaiter(Drama::Core::Time::IMonotonicClock& clock) noexcept
        : m_clock(&clock)
    {
        // 1) WaitableTimer を作成（失敗したらフォールバックでSleep）
        m_timer = ::CreateWaitableTimerExW(
            nullptr,
            nullptr,
            CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
            TIMER_ALL_ACCESS);

        // 2) 失敗は許容（m_timer=nullptrで動かす）
    }

    WinWaiter::~WinWaiter() noexcept
    {
        // 1) ハンドル破棄
        if (m_timer != nullptr)
        {
            ::CloseHandle(m_timer);
            m_timer = nullptr;
        }
    }

    void WinWaiter::sleep_for_coarse_ms(uint32_t ms) noexcept
    {
        // 1) 0は何もしない
        if (ms == 0)
        {
            return;
        }

        // 2) WaitableTimer があるならそちら（相対時間・100ns単位）
        if (m_timer != nullptr)
        {
            LARGE_INTEGER due{};
            const int64_t hundredNs = -static_cast<int64_t>(ms) * 10'000LL; // negative = relative
            due.QuadPart = hundredNs;

            const BOOL okSet = ::SetWaitableTimer(m_timer, &due, 0, nullptr, nullptr, FALSE);
            if (okSet != FALSE)
            {
                (void)::WaitForSingleObject(m_timer, INFINITE);
                return;
            }
        }

        // 3) フォールバック
        ::Sleep(ms);
    }

    void WinWaiter::sleep_for(Drama::Core::Time::DurationNs durationNs) noexcept
    {
        // 1) 負値/0は無視
        if (durationNs <= 0)
        {
            return;
        }

        // 2) WaitableTimer があるなら ns を直接（100ns単位へ切り上げ）
        //    ※ここがキモ：1ms未満でもブロックできる。スピン不要。
        if (m_timer != nullptr)
        {
            const int64_t hundredNs = ns_to_100ns_ceil(durationNs);
            if (hundredNs > 0)
            {
                LARGE_INTEGER due{};
                due.QuadPart = -hundredNs; // negative = relative

                const BOOL okSet = ::SetWaitableTimer(m_timer, &due, 0, nullptr, nullptr, FALSE);
                if (okSet != FALSE)
                {
                    (void)::WaitForSingleObject(m_timer, INFINITE);
                    return;
                }
            }
        }

        // 3) フォールバック：Sleep(ms) しかないので ms に切り上げて必ず「それ以上」寝る
        const uint32_t ms = ns_to_ms_ceil_u32(durationNs);
        if (ms == 0)
        {
            // 1ms未満でも Sleep(1) に丸める（std::this_thread の挙動に寄せる＝少なくとも寝る）
            ::Sleep(1);
            return;
        }

        sleep_for_coarse_ms(ms);
    }

    void WinWaiter::sleep_until(Drama::Core::Time::TickNs targetTickNs) noexcept
    {
        // 1) clockがないのは設計ミスだが、例外禁止なので何もしない
        if (m_clock == nullptr)
        {
            return;
        }

        // 2) 現在時刻
        const Drama::Core::Time::TickNs now = m_clock->now_tick();

        // 3) 既に過ぎているなら終わり
        if (targetTickNs <= now)
        {
            return;
        }

        // 4) 残りを1回だけ寝る（ループ禁止）
        const Drama::Core::Time::DurationNs remaining = targetTickNs - now;
        sleep_for(remaining);
    }
}

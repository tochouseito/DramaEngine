#pragma once

#include <cstdint>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "Core/Time/IWaiter.h"
#include "Core/Time/IMonotonicClock.h"

namespace Drama::Platform::Win::Time
{
    class WinWaiter final : public Drama::Core::Time::IWaiter
    {
    public:
        explicit WinWaiter(Drama::Core::Time::IMonotonicClock& clock) noexcept;
        ~WinWaiter() noexcept override;

        void sleep_for(Drama::Core::Time::DurationNs durationNs) noexcept override;
        void sleep_until(Drama::Core::Time::TickNs targetTickNs) noexcept override;
        void relax() noexcept override;
    private:
        void sleep_for_coarse_ms(uint32_t ms) noexcept;

    private:
        Drama::Core::Time::IMonotonicClock* m_clock = nullptr;
        HANDLE m_timer = nullptr;
    };
}

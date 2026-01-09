#pragma once

#include "Core/Time/IMonotonicClock.h"

namespace Drama::Platform::Win::Time
{
    class WinQpcClock final : public Drama::Core::Time::IMonotonicClock
    {
    public:
        WinQpcClock() noexcept;

        Drama::Core::Time::TickNs now_tick() noexcept override;

    private:
        int64_t m_freq = 0;
    };
}

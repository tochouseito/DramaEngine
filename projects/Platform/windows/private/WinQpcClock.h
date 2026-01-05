#pragma once

#include "Core/Time/IClock.h"

namespace Drama::Platform::Time
{
    class WinQpcClock final : public Drama::Core::Time::IClock
    {
    public:
        WinQpcClock() noexcept;

        Drama::Core::Time::Tick now() noexcept override;

    private:
        int64_t m_freq = 0;
    };
}

#pragma once
#include "Core/Time/TimeTypes.h"

namespace Drama::Core::Time
{
    class IMonotonicClock
    {
    public:
        virtual ~IMonotonicClock() noexcept = default;

        virtual TickNs now_tick() noexcept = 0;
    };
}

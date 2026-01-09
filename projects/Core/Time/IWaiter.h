#pragma once
#include "Core/Time/TimeTypes.h"

namespace Drama::Core::Time
{
    class IWaiter
    {
    public:
        virtual ~IWaiter() noexcept = default;

        virtual void sleep_for(DurationNs durationNs) noexcept = 0;
        virtual void sleep_until(TickNs targetTickNs) noexcept = 0;
    };
}

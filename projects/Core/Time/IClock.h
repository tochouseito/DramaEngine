#pragma once

#include <cstdint>

namespace Drama::Core::Time
{
    using Tick = int64_t; // ナノ秒単位

    class IClock
    {
    public:
        virtual ~IClock() = default;

        // 1) 現在時刻（ナノ秒単位のTick）を返す
        virtual Tick now() noexcept = 0;
    };
}

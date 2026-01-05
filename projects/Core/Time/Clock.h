#pragma once

#include <cstdint>
#include "Core/Time/IClock.h"

namespace Drama::Core::Time
{
    class Clock final
    {
    public:
        explicit Clock(IClock& impl) noexcept
            : m_impl(&impl)
        {
            // 1) nullは禁止（参照で受けてるので通常は起きない）
        }

        Tick now() const noexcept
        {
            // 1) 実装から現在Tickを取得
            return m_impl->now();
        }

        static double ticks_to_seconds(Tick ticks) noexcept
        {
            // 1) Tickはナノ秒とする
            // 2) 秒へ変換
            return static_cast<double>(ticks) / 1'000'000'000.0;
        }

        static Tick seconds_to_ticks(double seconds) noexcept
        {
            // 1) 秒からナノ秒へ変換
            const double ns = seconds * 1'000'000'000.0;

            // 2) Tickへ（丸め規約は必要なら後で統一）
            return static_cast<Tick>(ns);
        }

    private:
        IClock* m_impl = nullptr;
    };
}

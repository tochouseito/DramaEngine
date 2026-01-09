#pragma once

// === C++ Standard Library ===
#include <cstdint>
#include <chrono>
#include <thread>

// === Core ===
#include "Core/Time/Clock.h"
#include "Core/Time/Timer.h"
#include "Core/Time/IWaiter.h"

namespace Drama::Core::Time
{
    /// @brief フレームカウンター
    class FrameCounter final
    {
    public:
        explicit FrameCounter(const Clock& clock, const IWaiter& waiter) noexcept
            : m_clock(&clock)
            , m_timer(clock)
            , m_waiter(&waiter)
        {
            // 1) 初期化
            m_timer.reset();
        }

        ~FrameCounter() noexcept = default;

        // --------------------
        // Main API (new)
        // --------------------
        void tick() noexcept
        {
            // 1) 初回のみ：基準点だけ作る
            if (m_initialized == false)
            {
                m_timer.reset();
                m_capBaseTick = m_clock->now();
                m_initialized = true;
                return;
            }

            // 2) FPS制限（待ちをdeltaに含めるため、lap前に待つ）
            if (m_maxFps > 0)
            {
                cap_fps_();
            }

            // 3) delta計測（待ち含む）
            //    lap_seconds() は内部の基準点(m_last)を更新する
            m_deltaTime = m_timer.lap_seconds();
            if (m_deltaTime > 0.0)
            {
                m_fps = 1.0 / m_deltaTime;
            }
            else
            {
                m_fps = 0.0;
            }

            // 4) 統計更新
            m_totalFrames += 1;
            m_produceFrame += 1;

            // 5) 次のcap判定の基準Tick更新
            m_capBaseTick = m_clock->now();
        }

        double delta_time() const noexcept
        {
            return m_deltaTime;
        }

        double fps() const noexcept
        {
            return m_fps;
        }

        void set_max_fps(std::uint32_t maxFps) noexcept
        {
            m_maxFps = maxFps;
        }

        void set_max_lead(std::uint32_t maxLead) noexcept
        {
            m_maxLead = maxLead;
        }

        std::uint32_t max_lead() const noexcept
        {
            return m_maxLead;
        }

        std::uint64_t total_frames() const noexcept
        {
            return m_totalFrames;
        }

        std::uint64_t produce_frame() const noexcept
        {
            return m_produceFrame;
        }

    private:
        void cap_fps_() noexcept
        {
            using micrs = std::chrono::microseconds;
            using nanos = std::chrono::nanoseconds;

            // 1) フレームレートピッタリの時間
            const micrs frameUs = static_cast<micrs>(1'000'000 / m_maxFps);
            // 2) スピン待ち時間（マイクロ秒）ここが長いほどCPU負荷が上がり、精度が上がる
            const micrs spinUs = static_cast<micrs>(2000);
            // 3) フレーム開始時刻
            const TickNs startTick = m_capBaseTick;
            const nanos startNs = static_cast<nanos>(startTick);
            const micrs startUs = std::chrono::duration_cast<micrs>(startNs);
            // 4) 理想的な次フレーム開始時刻
            const micrs targetUs = startUs + frameUs;

            TickNs nowTick = m_clock->now();
            const nanos nowNs = static_cast<nanos>(nowTick);
            const micrs now = std::chrono::duration_cast<micrs>(nowNs);

            micrs elapsedUs = now - startUs;

            // 5) スピン待ち
            if (elapsedUs < frameUs)
            {
                // 大半をsleepで止める
                micrs sleepUs = targetUs - spinUs;
                if (sleepUs > now)
                {
                    std::chrono::time_point<std::chrono::steady_clock, micrs> sleepTimePoint(sleepUs);
                    std::this_thread::sleep_until(sleepUs);
                    m_waiter->sleep_until()
                }
            }

            // 1) 目標フレーム時間(秒)
            const double targetSec = 1.0 / static_cast<double>(m_maxFps);

            // 2) 現在の経過(秒)（lapを壊さないためClock差分で見る）
            const Tick nowTick = m_clock->now();
            const double elapsedSec = Clock::ticks_to_seconds(nowTick - m_capBaseTick);

            if (elapsedSec >= targetSec)
            {
                // 3) 既に目標を満たしている
                return;
            }

            // 4) sleep + spin（最後だけyield）
            //    ここは「精度 vs CPU負荷」のトレードオフ。
            const std::chrono::microseconds spinUs(2000);

            const double remainSec = (targetSec - elapsedSec);
            const auto remainUs = std::chrono::microseconds(
                static_cast<std::int64_t>(remainSec * 1000.0 * 1000.0));

            if (remainUs > spinUs)
            {
                std::this_thread::sleep_for(remainUs - spinUs);
            }

            while (true)
            {
                const Tick t = m_clock->now();
                const double sec = Clock::ticks_to_seconds(t - m_capBaseTick);
                if (sec >= targetSec)
                {
                    break;
                }

                std::this_thread::yield();
            }
        }

    private:
        const Clock* m_clock = nullptr;
        const IWaiter* m_waiter = nullptr;
        Timer m_timer;

        bool m_initialized = false;

        // FPS cap判定用の基準（前フレーム終了付近のTick）
        TickNs m_capBaseTick = 0;

        double m_deltaTime = 0.0;
        double m_fps = 0.0;

        std::uint32_t m_maxFps = 60;
        std::uint32_t m_maxLead = 0; // 2枚→1, 3枚→2

        std::uint64_t m_totalFrames = 0;
        std::uint64_t m_produceFrame = 0;
    };
}

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
        explicit FrameCounter(const Clock& clock, IWaiter& waiter) noexcept
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
            // 1) cap無し
            if (m_maxFps == 0)
            {
                return;
            }

            // 2) 1フレーム(ns)（丸め）
            const int64_t frameNs = static_cast<int64_t>((1'000'000'000.0 / static_cast<double>(m_maxFps)) + 0.5);

            // 3) 追い込みスピン(ns)（ここはPC向け。まず200usから）
            const int64_t spinNs = 200'000LL;

            // 4) 初回：次フレーム予定を作る（位相固定の開始点）
            const int64_t now0 = m_clock->now();
            if (m_nextTickNs == 0)
            {
                m_nextTickNs = now0 + frameNs;
            }

            // 5) 既に遅れているなら、追いつく（遅れを引きずらない）
            //    ※「遅れたフレームを無理に取り戻す」のは無理なので、次の予定を作り直す
            if (now0 >= m_nextTickNs)
            {
                m_nextTickNs = now0 + frameNs;
                return;
            }

            // 6) 大半をsleep（あなたのWaiterでブロック）
            const int64_t sleepUntilNs = m_nextTickNs - spinNs;
            if (sleepUntilNs > now0)
            {
                m_waiter->sleep_until(sleepUntilNs);
            }

            // 7) 最後だけ短スピン（yieldは禁止。精度が落ちる）
            while (m_clock->now() < m_nextTickNs)
            {
                m_waiter->relax();
            }

            // 8) 次フレーム予定を位相固定で進める（ここが肝）
            m_nextTickNs += frameNs;
        }

    private:
        const Clock* m_clock = nullptr;
        IWaiter* m_waiter = nullptr;
        Timer m_timer;

        bool m_initialized = false;

        // FPS cap判定用の基準（前フレーム終了付近のTick）
        TickNs m_capBaseTick = 0;
        int64_t m_nextTickNs = 0;

        double m_deltaTime = 0.0;
        double m_fps = 0.0;

        std::uint32_t m_maxFps = 60;
        std::uint32_t m_maxLead = 0; // 2枚→1, 3枚→2

        std::uint64_t m_totalFrames = 0;
        std::uint64_t m_produceFrame = 0;
    };
}

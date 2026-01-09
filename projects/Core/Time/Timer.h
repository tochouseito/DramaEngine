#pragma once

#include <cstddef>
#include "Core/Time/Clock.h"

namespace Drama::Core::Time
{
    class Timer final
    {
    public:
        explicit Timer(const Clock& clock) noexcept
            : m_clock(&clock)
        {
            // 1) 初期化
            reset();
        }

        void reset() noexcept
        {
            // 1) 状態初期化
            m_running = false;
            m_elapsed = 0;
            m_start = 0;
            m_last = m_clock->now();
        }

        void start() noexcept
        {
            // 1) すでに動いているなら何もしない
            if (m_running)
            {
                return;
            }

            // 2) 開始Tickを記録
            m_start = m_clock->now();
            m_running = true;
        }

        void stop() noexcept
        {
            // 1) 動いていないなら何もしない
            if (!m_running)
            {
                return;
            }

            // 2) 経過を加算して停止
            const TickNs nowTick = m_clock->now();
            m_elapsed += (nowTick - m_start);
            m_running = false;
        }

        bool is_running() const noexcept
        {
            // 1) 動作中フラグを返す
            return m_running;
        }

        TickNs elapsed_ticks() const noexcept
        {
            // 1) 既に積算した分を基準にする
            TickNs total = m_elapsed;

            // 2) 動作中なら現在までの差分を足す
            if (m_running)
            {
                const TickNs nowTick = m_clock->now();
                total += (nowTick - m_start);
            }

            return total;
        }

        double elapsed_seconds() const noexcept
        {
            // 1) Tickを取得
            const TickNs ticks = elapsed_ticks();

            // 2) 秒へ変換
            return Clock::ticks_to_seconds(ticks);
        }

        // フレーム計測用：前回呼び出しからの差分Tick（動作中/停止中に関係なく計測）
        TickNs lap_ticks() noexcept
        {
            // 1) 現在Tickを取得
            const TickNs nowTick = m_clock->now();

            // 2) 前回との差分を計算して更新
            const TickNs dt = nowTick - m_last;
            m_last = nowTick;

            return dt;
        }

        double lap_seconds() noexcept
        {
            // 1) 差分Tickを得る
            const TickNs dt = lap_ticks();

            // 2) 秒へ変換
            return Clock::ticks_to_seconds(dt);
        }

    private:
        const Clock* m_clock = nullptr;

        bool m_running = false;
        TickNs m_start = 0;
        TickNs m_elapsed = 0;

        TickNs m_last = 0; // lap用
    };
}

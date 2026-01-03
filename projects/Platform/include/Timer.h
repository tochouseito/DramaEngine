#pragma once
// === C++ Standard Library ===
#include <chrono>

namespace Drama::Platform
{
    class Timer final
    {
    public:
        using Clock = std::chrono::steady_clock;///< 単調増加クロック
        using TimePoint = Clock::time_point;///< 時間点,記録用
        using Duration = std::chrono::duration<double>;///< 経過時間,秒単位
        using Nanoseconds = std::chrono::nanoseconds;// ナノ秒
        using Microseconds = std::chrono::microseconds;// マイクロ秒
        using Milliseconds = std::chrono::milliseconds;// ミリ秒
        using Seconds = std::chrono::seconds;// 秒

        /// @brief コンストラクタ
        Timer() noexcept
        {
            reset();
        }
        /// @brief デストラクタ
        ~Timer() noexcept = default;
        /// @brief リセット
        void reset() noexcept
        {
            m_isRunning = false;
            m_elapsed = Duration::zero();
            m_start = TimePoint{};
            m_end = Clock::now();
        }
        /// @brief 記録開始
        void start() noexcept
        {
            if (!m_isRunning)
            {
                m_start = Clock::now();
                m_isRunning = true;
            }
        }
        /// @brief 記録停止
        void stop() noexcept
        {
            if (m_isRunning)
            {
                m_elapsed += Clock::now() - m_start;
                m_isRunning = false;
                m_end = Clock::now();
            }
        }
        /// @brief 開始からの経過時間を秒単位で取得
        /// @return 経過時間(秒) 
        double elapsed_seconds() const noexcept
        {
            auto total = m_elapsed;
            if (m_isRunning)
            {
                total += (Clock::now() - m_start);
            }
            return total.count();
        }
        Seconds elapsed_seconds_duration() const noexcept
        {
            auto total = m_elapsed;
            if (m_isRunning)
            {
                total += (Clock::now() - m_start);
            }
            return std::chrono::duration_cast<Seconds>(total);
        }
        /// @brief 経過時間をミリ秒単位で返します
        /// @return 経過時間をミリ秒単位で表す
        double elapsed_milliseconds() const noexcept
        {
            auto total = m_elapsed;
            if (m_isRunning) total += (Clock::now() - m_start);
            return total.count() * 1000.0;
        }
        Milliseconds elapsed_milliseconds_duration() const noexcept
        {
            auto total = m_elapsed;
            if (m_isRunning)
            {
                total += (Clock::now() - m_start);
            }
            return std::chrono::duration_cast<Milliseconds>(total);
        }
        /// @brief 経過時間をマイクロ秒単位で返します
        /// @return 経過時間をマイクロ秒単位で表す
        double elapsed_microseconds() const noexcept
        {
            auto total = m_elapsed;
            if (m_isRunning) total += (Clock::now() - m_start);
            return total.count() * 1'000'000.0;
        }
        Microseconds elapsed_microseconds_duration() const noexcept
        {
            auto total = m_elapsed;
            if (m_isRunning)
            {
                total += (Clock::now() - m_start);
            }
            return std::chrono::duration_cast<Microseconds>(total);
        }
        /// @brief 動作中かどうか取得
        bool is_running() const noexcept { return m_isRunning; }
        /// @brief 開始時間点取得
        TimePoint start_time() const noexcept { return m_start; }

    private:
        TimePoint m_start{};///< 開始時間点
        TimePoint m_end{};///< 終了時間点
        bool m_isRunning = false;///< 動作中フラグ
        Duration m_elapsed{};///< 経過時間
    };
}


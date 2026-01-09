#pragma once
#include <thread>
#include <chrono>

class RegTimer final
{
public:
    using Clock = std::chrono::steady_clock;///< 単調増加クロック
    using Time_Point = Clock::time_point;///< 時間点,記録用
    using Duration = std::chrono::duration<double>;///< 経過時間,秒単位
    using nanos = std::chrono::nanoseconds;// ナノ秒
    using micrs = std::chrono::microseconds;// マイクロ秒
    using millis = std::chrono::milliseconds;// ミリ秒
    using secs = std::chrono::seconds;// 秒

    /// @brief コンストラクタ
    RegTimer() noexcept
    {
        Reset();
    }
    /// @brief デストラクタ
    ~RegTimer() noexcept = default;
    /// @brief リセット
    void Reset() noexcept
    {
        m_Running = false;
        m_Elapsed = Duration::zero();
        m_Start = Time_Point{};
        m_End = Clock::now();
    }
    /// @brief 記録開始
    void Start() noexcept
    {
        if (!m_Running)
        {
            m_Start = Clock::now();
            m_Running = true;
        }
    }
    /// @brief 記録停止
    void Stop() noexcept
    {
        if (m_Running)
        {
            m_Elapsed += Clock::now() - m_Start;
            m_Running = false;
            m_End = Clock::now();
        }
    }
    /// @brief 開始からの経過時間を秒単位で取得
    /// @return 経過時間(秒) 
    double ElapsedSeconds() noexcept
    {
        auto total = m_Elapsed;
        if (m_Running)
        {
            total += (Clock::now() - m_Start);
        }
        return total.count();
    }
    secs ElapsedSecondsDuration() noexcept
    {
        auto total = m_Elapsed;
        if (m_Running)
        {
            total += (Clock::now() - m_Start);
        }
        return std::chrono::duration_cast<secs>(total);
    }
    /// @brief 経過時間をミリ秒単位で返します
    /// @return 経過時間をミリ秒単位で表す
    double ElapsedMilliseconds() noexcept
    {
        auto total = m_Elapsed;
        if (m_Running) total += (Clock::now() - m_Start);
        return total.count() * 1000.0;
    }
    millis ElapsedMillisecondsDuration() noexcept
    {
        auto total = m_Elapsed;
        if (m_Running)
        {
            total += (Clock::now() - m_Start);
        }
        return std::chrono::duration_cast<millis>(total);
    }
    /// @brief 経過時間をマイクロ秒単位で返します
    /// @return 経過時間をマイクロ秒単位で表す
    double ElapsedMicroseconds() noexcept
    {
        auto total = m_Elapsed;
        if (m_Running) total += (Clock::now() - m_Start);
        return total.count() * 1'000'000.0;
    }
    micrs ElapsedMicrosecondsDuration() noexcept
    {
        auto total = m_Elapsed;
        if (m_Running)
        {
            total += (Clock::now() - m_Start);
        }
        return std::chrono::duration_cast<micrs>(total);
    }
    /// @brief 動作中かどうか取得
    bool IsRunning() const noexcept { return m_Running; }
    /// @brief 開始時間点取得
    Time_Point StartTime() const noexcept { return m_Start; }

private:
    Time_Point m_Start{};///< 開始時間点
    Time_Point m_End{};///< 終了時間点
    bool m_Running = false;///< 動作中フラグ
    Duration m_Elapsed{};///< 経過時間
};

/// @brief フレームカウンター
class RegFrameCounter final
{
public:
    /// @brief コンストラクタ
    RegFrameCounter() noexcept = default;
    /// @brief デストラクタ
    ~RegFrameCounter() noexcept = default;

    void Tick() noexcept
    {
        using Clock = RegTimer::Clock;
        using TimePoint = RegTimer::Time_Point;
        using micrs = RegTimer::micrs;

        // 初回のみ
        if (!m_Initialized)
        {
            m_Timer.Start();
            m_Initialized = true;
            return;
        }

        if (m_MaxFPS > 0)
        {
            // フレームレートピッタリの時間
            const micrs frameUs = micrs(static_cast<int64_t>(1'000'000.0 / static_cast<double>(m_MaxFPS)));
            // スピン待ち時間（マイクロ秒）
            // ここが長いほどCPU負荷が上がるが、精度が上がる
            const micrs spinUs = micrs(2000);
            // フレーム開始時刻
            const RegTimer::Time_Point start = m_Timer.StartTime();
            // 理想的な次フレーム開始時刻
            const TimePoint target = start + frameUs;

            auto now = Clock::now();
            auto elapsed = std::chrono::duration_cast<micrs>(now - start);

            // スピン待ち
            if (elapsed < frameUs)
            {
                // 大半をsleep_untilで止める
                TimePoint sleepUntil = target - spinUs;
                if (sleepUntil > now)
                {
                    std::this_thread::sleep_until(sleepUntil);
                    now = Clock::now();
                }
                // 最後の1msをスピン待ち
                while (Clock::now() < target)
                {
                    std::this_thread::yield();
                }
            }
        }

        m_DeltaTime = m_Timer.ElapsedSeconds();
        m_FPS = (m_DeltaTime > 0.0) ? (1.0 / m_DeltaTime) : 0.0;
        m_TotalFrames++;

        // 計測開始
        m_Timer.Stop();
        m_Timer.Reset();
        m_Timer.Start();
    }

    /// @brief デルタタイム取得
    double DeltaTime() const noexcept
    {
        return m_DeltaTime;
    }

    /// @brief フレームレート取得
    double FPS() const noexcept
    {
        return m_FPS;
    }

    /// @brief 最大フレームレート設定
    /// @brief 0以下で無制限
    /// @param max_fps 
    void SetMaxFPS(uint32_t max_fps) noexcept
    {
        m_MaxFPS = max_fps;
    }

    void SetMaxLead(uint32_t max_lead) noexcept
    {
        m_MaxLead = max_lead;
    }
    uint32_t GetMaxLead() const noexcept
    {
        return m_MaxLead;
    }
    uint64_t m_TotalFrames{};///< 総フレーム数
    uint64_t m_ProduceFrame = 0; // Update/Render をキック
private:
    RegTimer m_Timer;///< タイマー
    bool m_Initialized{ false };//< 初期化フラグ
    double m_DeltaTime{};///< 1 / フレーム時間(秒)
    double m_FPS{};///< フレームレート
    uint32_t m_MaxFPS{ 60 };//< 最大フレームレート
    uint32_t m_MaxLead = 0;  // 2枚→1, 3枚→2
};

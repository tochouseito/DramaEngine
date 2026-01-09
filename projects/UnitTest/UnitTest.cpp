#include <condition_variable>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <conio.h>

#include "Platform/public/Platform.h"
#include "Core/Time/FrameCounter.h"

class Logger final
{
public:
    void log_line(const std::string& line)
    {
        // 1) 排他して出力が混ざらないようにする
        std::lock_guard<std::mutex> lock(m_mutex);
        std::cout << line << std::endl;
    }

private:
    std::mutex m_mutex;
};

class FrameJob final
{
public:
    using JobFunc = std::function<void(uint64_t, uint32_t)>;

    void start(JobFunc func)
    {
        // 1) 実行関数を保持
        // 2) ループスレッドを開始
        m_func = std::move(func);
        m_thread = std::thread([this]()
            {
                uint64_t currentFrame = 0;
                while (true)
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_cv.wait(lock, [&]()
                        {
                            return m_exit || m_requestedFrame > currentFrame;
                        });

                    if (m_exit)
                    {
                        break;
                    }

                    currentFrame = m_requestedFrame;
                    const uint32_t index = m_paramIndex;
                    lock.unlock();

                    m_func(currentFrame, index);

                    lock.lock();
                    m_finishedFrame = currentFrame;
                    lock.unlock();
                    m_cv.notify_all();
                }
            });
    }

    void kick(uint64_t frameNo, uint32_t index)
    {
        // 1) 最新のリクエストを更新
        // 2) スレッドに通知
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_requestedFrame = frameNo;
            m_paramIndex = index;
        }
        m_cv.notify_one();
    }

    uint64_t get_finished_frame() const
    {
        // 1) 排他して完了フレームを取得
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_finishedFrame;
    }

    void stop()
    {
        // 1) 終了フラグを立てる
        // 2) スレッドを停止する
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_exit = true;
        }
        m_cv.notify_all();
        if (m_thread.joinable())
        {
            m_thread.join();
        }
    }

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::thread m_thread;
    JobFunc m_func;
    uint64_t m_requestedFrame = 0;
    uint64_t m_finishedFrame = 0;
    uint32_t m_paramIndex = 0;
    bool m_exit = false;
};

enum class PipelineMode
{
    Fixed,
    Mailbox,
    Backpressure,
};

struct AppConfig final
{
    uint32_t bufferCount = 3;
    uint32_t maxFps = 60;
    PipelineMode mode = PipelineMode::Fixed;
    bool hasFrameLimit = false;
    uint64_t totalFrames = 0;
};

static bool try_parse_mode(const std::string& text, PipelineMode& mode)
{
    // 1) 文字列を比較してモードに変換
    // 2) 不一致なら false
    if (text == "fixed")
    {
        mode = PipelineMode::Fixed;
        return true;
    }
    if (text == "mailbox")
    {
        mode = PipelineMode::Mailbox;
        return true;
    }
    if (text == "backpressure")
    {
        mode = PipelineMode::Backpressure;
        return true;
    }

    return false;
}

static const char* mode_to_string(PipelineMode mode)
{
    // 1) モード名を返す
    switch (mode)
    {
        case PipelineMode::Fixed:
            return "fixed";
        case PipelineMode::Mailbox:
            return "mailbox";
        case PipelineMode::Backpressure:
            return "backpressure";
    }

    return "unknown";
}

static bool try_parse_uint64(const std::string& text, uint64_t& value)
{
    // 1) 数字以外が含まれていないか確認
    // 2) 10進数として安全に変換
    if (text.empty())
    {
        return false;
    }

    uint64_t result = 0;
    constexpr uint64_t maxValue = std::numeric_limits<uint64_t>::max();
    for (char ch : text)
    {
        if (ch < '0' || ch > '9')
        {
            return false;
        }
        const uint64_t digit = static_cast<uint64_t>(ch - '0');
        if (result > (maxValue - digit) / 10)
        {
            return false;
        }
        result = result * 10 + digit;
    }

    value = result;
    return true;
}

static bool try_parse_uint32(const std::string& text, uint32_t& value)
{
    // 1) 64bitで受けて範囲内か確認
    // 2) 32bitに代入
    uint64_t temp = 0;
    if (!try_parse_uint64(text, temp))
    {
        return false;
    }
    if (temp > std::numeric_limits<uint32_t>::max())
    {
        return false;
    }
    value = static_cast<uint32_t>(temp);
    return true;
}

static bool try_parse_config(int argc, char** argv, AppConfig& config, Logger& logger)
{
    // 1) 引数を走査して設定を上書き
    // 2) 不正なら使い方を表示して終了
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        const std::string buffersPrefix = "--buffers=";
        const std::string framesPrefix = "--frames=";
        const std::string fpsPrefix = "--fps=";
        const std::string modePrefix = "--mode=";

        if (arg.rfind(buffersPrefix, 0) == 0)
        {
            const std::string valueText = arg.substr(buffersPrefix.size());
            uint32_t bufferCount = 0;
            if (!try_parse_uint32(valueText, bufferCount))
            {
                logger.log_line("buffers の値が不正です。例: --buffers=2");
                return false;
            }
            if (bufferCount != 2 && bufferCount != 3)
            {
                logger.log_line("buffers は 2 か 3 だけ対応しています。");
                return false;
            }
            config.bufferCount = bufferCount;
        }
        else if (arg.rfind(framesPrefix, 0) == 0)
        {
            const std::string valueText = arg.substr(framesPrefix.size());
            uint64_t frames = 0;
            if (!try_parse_uint64(valueText, frames))
            {
                logger.log_line("frames の値が不正です。例: --frames=12");
                return false;
            }
            if (frames == 0)
            {
                logger.log_line("frames は 1 以上にしてください。");
                return false;
            }
            config.hasFrameLimit = true;
            config.totalFrames = frames;
        }
        else if (arg.rfind(fpsPrefix, 0) == 0)
        {
            const std::string valueText = arg.substr(fpsPrefix.size());
            uint32_t fps = 0;
            if (!try_parse_uint32(valueText, fps))
            {
                logger.log_line("fps の値が不正です。例: --fps=60");
                return false;
            }
            config.maxFps = fps;
        }
        else if (arg.rfind(modePrefix, 0) == 0)
        {
            const std::string valueText = arg.substr(modePrefix.size());
            PipelineMode mode = PipelineMode::Fixed;
            if (!try_parse_mode(valueText, mode))
            {
                logger.log_line("mode は fixed/mailbox/backpressure のいずれかです。");
                return false;
            }
            config.mode = mode;
        }
        else
        {
            logger.log_line("不明な引数です: " + arg);
            return false;
        }
    }

    return true;
}

class FramePipelineDemo final
{
public:
    FramePipelineDemo(const AppConfig& config, Logger& logger)
        : m_config(config), m_logger(logger)
        , m_platform()
        , m_clock(*m_platform.clock())
        , m_waiter(m_platform.waiter())
        , m_frameCounter(m_clock, *m_waiter)
    {
        // 1) 初期化はメンバ初期化リストで完結させる
    }

    void run()
    {
        // 1) FPS上限と先行数の設定
        // 2) 初期バッファを埋める
        // 3) Update/Render ジョブを起動
        // 4) モード別にフレームを進める
        m_frameCounter.set_max_fps(m_config.maxFps);
        m_frameCounter.set_max_lead(m_config.bufferCount - 1);
        fill_buffers("Warmup", 0);

        m_updateJob.start([this](uint64_t frameNo, uint32_t index)
            {
                do_update(frameNo, index);
            });
        m_renderJob.start([this](uint64_t frameNo, uint32_t index)
            {
                do_render(frameNo, index);
            });

        switch (m_config.mode)
        {
            case PipelineMode::Fixed:
                run_fixed();
                break;
            case PipelineMode::Mailbox:
                run_mailbox();
                break;
            case PipelineMode::Backpressure:
                run_backpressure();
                break;
        }

        m_updateJob.stop();
        m_renderJob.stop();
    }

private:
    void compute_indices(uint64_t frameNo, uint32_t bufferCount, uint32_t& updateIndex,
        uint32_t& renderIndex, uint32_t& presentIndex)
    {
        // 1) presentIndex を算出
        // 2) update/render をオフセットで算出
        const uint64_t baseFrame = frameNo + m_backBufferBase;
        presentIndex = static_cast<uint32_t>(baseFrame % bufferCount);
        renderIndex = (presentIndex + bufferCount - 2) % bufferCount;
        updateIndex = (presentIndex + bufferCount - 1) % bufferCount;
    }

    void present_frame(uint64_t frameNo)
    {
        // 1) インデックスを計算
        // 2) FPS制御とログ出力
        uint32_t updateIndex = 0;
        uint32_t renderIndex = 0;
        uint32_t presentIndex = 0;
        compute_indices(frameNo, m_config.bufferCount, updateIndex, renderIndex, presentIndex);

        m_frameCounter.tick();
        m_logger.log_line("[Present] frame=" + std::to_string(frameNo) +
            " updateIndex=" + std::to_string(updateIndex) +
            " renderIndex=" + std::to_string(renderIndex) +
            " presentIndex=" + std::to_string(presentIndex) +
            " base=" + std::to_string(m_backBufferBase) +
            " fps=" + std::to_string(m_frameCounter.fps()));
    }

    void apply_resize_for_next_frame(uint64_t nextFrameNo)
    {
        // 1) 次フレームの presentIndex を 0 に合わせる
        // 2) 基準更新をログに出す
        const uint32_t bufferCount = m_config.bufferCount;
        const uint32_t mod = static_cast<uint32_t>(nextFrameNo % bufferCount);
        m_backBufferBase = (bufferCount - mod) % bufferCount;

        m_logger.log_line("[Resize] nextFrame=" + std::to_string(nextFrameNo) +
            " base=" + std::to_string(m_backBufferBase));
    }

    void fill_buffers(const char* tag, uint64_t frameNo)
    {
        // 1) 全バッファを順番に埋めなおす
        // 2) タグを付けてログに残す
        const std::string prefix = "[" + std::string(tag) + "Fill] ";
        for (uint32_t i = 0; i < m_config.bufferCount; ++i)
        {
            m_logger.log_line(prefix + "frame=" + std::to_string(frameNo) +
                " index=" + std::to_string(i));
            do_update(frameNo, i);
        }
    }

    void poll_resize_request()
    {
        // 1) 入力があれば読み取る
        // 2) リサイズ要求をフラグに反映
        while (_kbhit() != 0)
        {
            const int ch = _getch();
            if (ch == 'r' || ch == 'R')
            {
                m_resizePending = true;
                m_logger.log_line("[ResizeRequest]");
            }
        }
    }

    void run_fixed()
    {
        // 1) 変数初期化
        // 2) 先行上限で Update/Render をキック
        // 3) 完了したフレームを順に Present
        uint64_t produceFrame = 0;
        uint64_t totalFrame = 0;
        const uint64_t maxLead = static_cast<uint64_t>(m_config.bufferCount - 1);
        const bool hasLimit = m_config.hasFrameLimit;
        const uint64_t frameLimit = hasLimit ? m_config.totalFrames : std::numeric_limits<uint64_t>::max();

        while (true)
        {
            poll_resize_request();

            if (m_resizePending && produceFrame == totalFrame)
            {
                apply_resize_for_next_frame(totalFrame);
                fill_buffers("Resize", totalFrame);
                m_resizePending = false;
            }

            const bool canProduce = !m_resizePending && produceFrame < frameLimit;
            if (canProduce && (produceFrame - totalFrame) < maxLead)
            {
                uint32_t updateIndex = 0;
                uint32_t renderIndex = 0;
                uint32_t presentIndex = 0;
                compute_indices(produceFrame, m_config.bufferCount, updateIndex, renderIndex, presentIndex);
                (void)presentIndex;

                m_updateJob.kick(produceFrame, updateIndex);
                m_renderJob.kick(produceFrame, renderIndex);
                ++produceFrame;
            }

            const bool canPresent = m_updateJob.get_finished_frame() >= totalFrame &&
                m_renderJob.get_finished_frame() >= totalFrame;
            if (canPresent)
            {
                present_frame(totalFrame);
                ++totalFrame;

                if (m_resizePending && produceFrame == totalFrame)
                {
                    apply_resize_for_next_frame(totalFrame);
                    fill_buffers("Resize", totalFrame);
                    m_resizePending = false;
                }
            }
            else
            {
                m_waiter->relax();
            }

            if (hasLimit && totalFrame >= m_config.totalFrames)
            {
                break;
            }
        }
    }

    void run_mailbox()
    {
        // 1) 変数初期化
        // 2) 先行上限で Update/Render をキック
        // 3) 最新完成フレームのみ Present
        uint64_t produceFrame = 0;
        uint64_t lastPresentedFrame = 0;
        bool hasPresented = false;
        const uint64_t maxLead = static_cast<uint64_t>(m_config.bufferCount - 1);
        const bool hasLimit = m_config.hasFrameLimit;
        const uint64_t frameLimit = hasLimit ? m_config.totalFrames : std::numeric_limits<uint64_t>::max();

        while (true)
        {
            poll_resize_request();

            if (m_resizePending && !hasPresented && produceFrame == 0)
            {
                apply_resize_for_next_frame(0);
                fill_buffers("Resize", 0);
                m_resizePending = false;
            }

            const uint64_t presentBase = hasPresented ? lastPresentedFrame : 0;
            const bool canProduce = !m_resizePending && produceFrame < frameLimit;
            if (canProduce && (produceFrame - presentBase) < maxLead)
            {
                uint32_t updateIndex = 0;
                uint32_t renderIndex = 0;
                uint32_t presentIndex = 0;
                compute_indices(produceFrame, m_config.bufferCount, updateIndex, renderIndex, presentIndex);
                (void)presentIndex;

                m_updateJob.kick(produceFrame, updateIndex);
                m_renderJob.kick(produceFrame, renderIndex);
                ++produceFrame;
            }

            const uint64_t updateFinished = m_updateJob.get_finished_frame();
            const uint64_t renderFinished = m_renderJob.get_finished_frame();
            const uint64_t readyFrame = (updateFinished < renderFinished) ? updateFinished : renderFinished;

            bool didPresent = false;
            if (!hasPresented || readyFrame > lastPresentedFrame)
            {
                present_frame(readyFrame);
                lastPresentedFrame = readyFrame;
                hasPresented = true;
                didPresent = true;
            }

            bool didResize = false;
            if (m_resizePending && hasPresented)
            {
                const bool noInFlight = (lastPresentedFrame + 1) == produceFrame;
                const bool workersDone = updateFinished >= lastPresentedFrame &&
                    renderFinished >= lastPresentedFrame;
                if (noInFlight && workersDone)
                {
                    apply_resize_for_next_frame(lastPresentedFrame + 1);
                    fill_buffers("Resize", lastPresentedFrame + 1);
                    m_resizePending = false;
                    didResize = true;
                }
            }

            if (!didPresent && !didResize)
            {
                m_waiter->relax();
            }

            if (hasLimit && hasPresented && (lastPresentedFrame + 1) >= m_config.totalFrames)
            {
                break;
            }
        }
    }

    void run_backpressure()
    {
        // 1) 変数初期化
        // 2) 1フレームずつ Update/Render をキック
        // 3) 完了したら Present して次へ進む
        uint64_t currentFrame = 0;
        bool inFlight = false;
        const bool hasLimit = m_config.hasFrameLimit;
        const uint64_t frameLimit = hasLimit ? m_config.totalFrames : std::numeric_limits<uint64_t>::max();

        while (true)
        {
            poll_resize_request();

            if (!inFlight)
            {
                if (m_resizePending)
                {
                    apply_resize_for_next_frame(currentFrame);
                    fill_buffers("Resize", currentFrame);
                    m_resizePending = false;
                }

                const bool canProduce = currentFrame < frameLimit;
                if (!canProduce)
                {
                    break;
                }

                uint32_t updateIndex = 0;
                uint32_t renderIndex = 0;
                uint32_t presentIndex = 0;
                compute_indices(currentFrame, m_config.bufferCount, updateIndex, renderIndex, presentIndex);
                (void)presentIndex;

                m_updateJob.kick(currentFrame, updateIndex);
                m_renderJob.kick(currentFrame, renderIndex);
                inFlight = true;
            }

            const bool canPresent = m_updateJob.get_finished_frame() >= currentFrame &&
                m_renderJob.get_finished_frame() >= currentFrame;
            if (canPresent)
            {
                present_frame(currentFrame);
                ++currentFrame;
                inFlight = false;
            }
            else
            {
                m_waiter->relax();
            }
        }
    }

    void do_update(uint64_t frameNo, uint32_t index)
    {
        // 1) 擬似負荷を入れて並列性を見える化
        // 2) ログは Present に集約する
        frameNo;
        index;
        spin_wait_ms(16);
    }

    void do_render(uint64_t frameNo, uint32_t index)
    {
        // 1) 擬似負荷を入れて並列性を見える化
        // 2) ログは Present に集約する
        frameNo;
        index;
        spin_wait_ms(16);
    }

    void spin_wait_ms(uint32_t milliseconds)
    {
        // 1) 目標Tickを計算
        // 2) 目標到達まで軽く回す
        const Drama::Core::Time::TickNs start = m_clock.now();
        const double seconds = static_cast<double>(milliseconds) / 1000.0;
        const Drama::Core::Time::TickNs target = start + Drama::Core::Time::Clock::seconds_to_ticks(seconds);

        while (m_clock.now() < target)
        {
            m_waiter->relax();
        }
    }

    AppConfig m_config;
    Logger& m_logger;
    Drama::Platform::System m_platform;
    Drama::Core::Time::Clock m_clock;
    Drama::Core::Time::IWaiter* m_waiter = nullptr;
    Drama::Core::Time::FrameCounter m_frameCounter;
    uint32_t m_backBufferBase = 0;
    bool m_resizePending = false;
    FrameJob m_updateJob;
    FrameJob m_renderJob;
};

int main(int argc, char** argv)
{
    // 1) 設定を読み取る
    // 2) モード別のデモを実行する
    if (true)
    {
        Logger logger;
        AppConfig config;
        config.maxFps = 0; // FPS制限無しデフォルト
        config.mode = PipelineMode::Backpressure; // Backpressure デフォルト
        if (!try_parse_config(argc, argv, config, logger))
        {
            logger.log_line("使い方: UnitTest.exe --buffers=2|3 [--frames=12] [--fps=60] [--mode=fixed|mailbox|backpressure]");
            return 1;
        }

        logger.log_line(std::string("FramePipeline Demo: ") + mode_to_string(config.mode));
        if (config.hasFrameLimit)
        {
            logger.log_line("buffers=" + std::to_string(config.bufferCount) +
                " frames=" + std::to_string(config.totalFrames));
        }
        else
        {
            logger.log_line("buffers=" + std::to_string(config.bufferCount) +
                " frames=unlimited");
        }
        logger.log_line("targetFps=" + std::to_string(config.maxFps));
        logger.log_line("resizeKey=R");

        FramePipelineDemo demo(config, logger);
        demo.run();
    }

    // 2) FrameCounter の単体動作確認ループ
    if (false)
    {
        Drama::Platform::System platform;
        Drama::Core::Time::Clock clock(*platform.clock());
        Drama::Core::Time::FrameCounter frameCounter(clock, *platform.waiter());
        frameCounter.set_max_fps(60);

        while (true)
        {
            frameCounter.tick();
            std::cout << "FPS: " << frameCounter.fps() << std::endl;
        }
    }

    return 0;
}

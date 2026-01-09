#include "pch.h"
#include "FramePipeline.h"

#include <utility>

#include "Core/IO/public/LogAssert.h"

namespace Drama::Frame
{
    bool FrameJob::start(ThreadFactory& factory, const char* name, JobFunc func)
    {
        // 1) 実行関数を保持
        // 2) ループスレッドを開始
        m_func = std::move(func);
        m_exit = false;
        m_finishedFrame = 0;
        m_queue.clear();

        Drama::Core::Threading::ThreadDesc desc{};
        if (name != nullptr)
        {
            desc.name = name;
        }

        std::unique_ptr<Thread> th{};
        const auto result = factory.create_thread(desc, &FrameJob::thread_entry, this, th);
        if (!result)
        {
            return false;
        }

        m_thread = std::move(th);
        return true;
    }
    void FrameJob::kick(uint64_t frameNo, uint32_t index)
    {
        // 1) 最新のリクエストを更新
        // 2) スレッドに通知
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push_back(Request{ frameNo, index });
        }
        m_cv.notify_one();
    }
    uint64_t FrameJob::get_finished_frame() const
    {
        // 1) 排他して状態を読む
        // 2) 完了フレームを返す
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_finishedFrame;
    }
    void FrameJob::stop()
    {
        // 1) 終了フラグを立てる
        // 2) スレッドを停止する
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_exit = true;
        }
        m_cv.notify_all();

        if (m_thread)
        {
            m_thread->request_stop();
            if (m_thread->joinable())
            {
                (void)m_thread->join();
            }
            m_thread.reset();
        }
    }
    uint32_t FrameJob::thread_entry(StopToken token, void* user) noexcept
    {
        // 1) this を取得
        // 2) ループを実行
        auto* self = static_cast<FrameJob*>(user);
        if (!self)
        {
            return 0;
        }
        return self->thread_loop(token);
    }
    uint32_t FrameJob::thread_loop(StopToken token) noexcept
    {
        // 1) フレーム要求を待つ
        // 2) 停止要求で抜ける
        uint64_t currentFrame = 0;
        while (!token.stop_requested())
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [&]()
                {
                    return m_exit || !m_queue.empty() || token.stop_requested();
                });

            if (m_exit || token.stop_requested())
            {
                break;
            }

            const Request req = m_queue.front();
            m_queue.pop_front();
            currentFrame = req.frameNo;
            const uint32_t index = req.index;
            lock.unlock();

            m_func(currentFrame, index);

            lock.lock();
            m_finishedFrame = currentFrame;
            lock.unlock();
            m_cv.notify_all();
        }

        return 0;
    }
    FramePipeline::~FramePipeline()
    {
        // 1) 実行中のジョブを停止する
        stop_jobs();
    }
    bool FramePipeline::start_pipeline()
    {
        // 1) 設定を検証して初期状態を整える
        // 2) 初期バッファを埋めてジョブを開始する
        Drama::Core::IO::LogAssert::assert((m_config.bufferCount >= 2), "bufferCount は 2 以上が必要です。");
        Drama::Core::IO::LogAssert::assert(static_cast<bool>(m_updateFunc), "Update 関数が未設定です。");
        Drama::Core::IO::LogAssert::assert(static_cast<bool>(m_renderFunc), "Render 関数が未設定です。");
        Drama::Core::IO::LogAssert::assert(static_cast<bool>(m_presentFunc), "Present 関数が未設定です。");

        m_frameCounter.set_max_fps(m_config.maxFps);
        m_frameCounter.set_max_lead(m_config.bufferCount - 1);
        m_maxLead = static_cast<uint64_t>(m_config.bufferCount - 1);
        m_backBufferBase = 0;
        m_fixedState = FixedState{};
        m_mailboxState = MailboxState{};
        m_backpressureState = BackpressureState{};

        fill_buffers(0);

        if (!m_updateJob.start(m_threadFactory, "UpdateJob", [this](uint64_t frameNo, uint32_t index)
            {
                m_updateFunc(frameNo, index);
            }))
        {
            Drama::Core::IO::LogAssert::assert(false, "UpdateJob の開始に失敗しました。");
            return false;
        }

        if (!m_renderJob.start(m_threadFactory, "RenderJob", [this](uint64_t frameNo, uint32_t index)
            {
                m_renderFunc(frameNo, index);
            }))
        {
            Drama::Core::IO::LogAssert::assert(false, "RenderJob の開始に失敗しました。");
            m_updateJob.stop();
            return false;
        }

        m_started = true;
        return true;
    }
    void FramePipeline::stop_jobs()
    {
        // 1) 起動済みジョブを停止する
        // 2) 起動状態を更新する
        m_updateJob.stop();
        m_renderJob.stop();
        m_started = false;
    }
    void FramePipeline::step()
    {
        // 1) 必要なら初期化する
        // 2) モード別に 1 ステップ進める
        // 3) 完了時はジョブを止める
        if (m_finished)
        {
            return;
        }

        if (!m_started)
        {
            if (!start_pipeline())
            {
                m_finished = true;
                return;
            }
        }

        bool isRunning = true;
        switch (m_config.mode)
        {
        case PipelineMode::Fixed:
            isRunning = step_fixed();
            break;
        case PipelineMode::Mailbox:
            isRunning = step_mailbox();
            break;
        case PipelineMode::Backpressure:
            isRunning = step_backpressure();
            break;
        }

        if (!isRunning)
        {
            stop_jobs();
            m_finished = true;
        }
    }
    void FramePipeline::compute_indices(uint64_t frameNo, uint32_t bufferCount, uint32_t& updateIndex, uint32_t& renderIndex, uint32_t& presentIndex)
    {
        // 1) presentIndex を算出
        // 2) update/render をオフセットで算出
        const uint64_t baseFrame = frameNo + m_backBufferBase;
        presentIndex = static_cast<uint32_t>(baseFrame % bufferCount);
        renderIndex = (presentIndex + bufferCount - 2) % bufferCount;
        updateIndex = (presentIndex + bufferCount - 1) % bufferCount;
    }
    void FramePipeline::present_frame(uint64_t frameNo)
    {
        // 1) インデックスを計算
        // 2) Present を呼び出して FPS 制御する
        uint32_t updateIndex = 0;
        uint32_t renderIndex = 0;
        uint32_t presentIndex = 0;
        compute_indices(frameNo, m_config.bufferCount, updateIndex, renderIndex, presentIndex);
        (void)updateIndex;
        (void)renderIndex;

        m_presentFunc(frameNo, presentIndex);
        m_frameCounter.tick();
    }
    void FramePipeline::apply_resize_for_next_frame(uint64_t nextFrameNo)
    {
        // 1) 次フレームの presentIndex を 0 に合わせる
        // 2) 基準更新をログに出す
        const uint32_t bufferCount = m_config.bufferCount;
        const uint32_t mod = static_cast<uint32_t>(nextFrameNo % bufferCount);
        m_backBufferBase = (bufferCount - mod) % bufferCount;
    }
    void FramePipeline::fill_buffers(uint64_t frameNo)
    {
        // 1) バッファ数を走査
        // 2) 全バッファを順番に埋めなおす
        for (uint32_t i = 0; i < m_config.bufferCount; ++i)
        {
            m_updateFunc(frameNo, i);
        }
    }
    void FramePipeline::poll_resize_request()
    {
        // 1) 外部イベントでリサイズ要求を受ける
        // 2) フラグを立てて次の安全なタイミングで反映する
        m_resizePending.store(true, std::memory_order_relaxed);
    }
    bool FramePipeline::step_fixed()
    {
        // 1) リサイズ要求と生成条件を確認する
        // 2) 生成できる場合は Update/Render をキックする
        // 3) Present 可能なら進める
        if (m_resizePending.load(std::memory_order_relaxed) &&
            m_fixedState.produceFrame == m_fixedState.totalFrame)
        {
            apply_resize_for_next_frame(m_fixedState.totalFrame);
            fill_buffers(m_fixedState.totalFrame);
            m_resizePending.store(false, std::memory_order_relaxed);
        }

        const bool canProduce = !m_resizePending.load(std::memory_order_relaxed);
        if (canProduce && (m_fixedState.produceFrame - m_fixedState.totalFrame) < m_maxLead)
        {
            uint32_t updateIndex = 0;
            uint32_t renderIndex = 0;
            uint32_t presentIndex = 0;
            compute_indices(m_fixedState.produceFrame, m_config.bufferCount, updateIndex, renderIndex, presentIndex);
            (void)presentIndex;

            m_updateJob.kick(m_fixedState.produceFrame, updateIndex);
            m_renderJob.kick(m_fixedState.produceFrame, renderIndex);
            ++m_fixedState.produceFrame;
        }

        const bool canPresent = m_updateJob.get_finished_frame() >= m_fixedState.totalFrame &&
            m_renderJob.get_finished_frame() >= m_fixedState.totalFrame;
        if (canPresent)
        {
            present_frame(m_fixedState.totalFrame);
            ++m_fixedState.totalFrame;

            if (m_resizePending.load(std::memory_order_relaxed) &&
                m_fixedState.produceFrame == m_fixedState.totalFrame)
            {
                apply_resize_for_next_frame(m_fixedState.totalFrame);
                fill_buffers(m_fixedState.totalFrame);
                m_resizePending.store(false, std::memory_order_relaxed);
            }
        }
        else
        {
            m_waiter.relax();
        }

        return true;
    }
    bool FramePipeline::step_mailbox()
    {
        // 1) 初期リサイズの安全点を確認する
        // 2) 先行上限内で Update/Render をキックする
        // 3) Present とリサイズ反映を進める
        if (m_resizePending.load(std::memory_order_relaxed) && !m_mailboxState.hasPresented &&
            m_mailboxState.produceFrame == 0)
        {
            apply_resize_for_next_frame(0);
            fill_buffers(0);
            m_resizePending.store(false, std::memory_order_relaxed);
        }

        const uint64_t presentBase = m_mailboxState.hasPresented ? m_mailboxState.lastPresentedFrame : 0;
        const bool canProduce = !m_resizePending.load(std::memory_order_relaxed);
        if (canProduce && (m_mailboxState.produceFrame - presentBase) < m_maxLead)
        {
            uint32_t updateIndex = 0;
            uint32_t renderIndex = 0;
            uint32_t presentIndex = 0;
            compute_indices(m_mailboxState.produceFrame, m_config.bufferCount, updateIndex, renderIndex, presentIndex);
            (void)presentIndex;

            m_updateJob.kick(m_mailboxState.produceFrame, updateIndex);
            m_renderJob.kick(m_mailboxState.produceFrame, renderIndex);
            ++m_mailboxState.produceFrame;
        }

        const uint64_t updateFinished = m_updateJob.get_finished_frame();
        const uint64_t renderFinished = m_renderJob.get_finished_frame();
        const uint64_t readyFrame = (updateFinished < renderFinished) ? updateFinished : renderFinished;

        bool didPresent = false;
        if (!m_mailboxState.hasPresented || readyFrame > m_mailboxState.lastPresentedFrame)
        {
            present_frame(readyFrame);
            m_mailboxState.lastPresentedFrame = readyFrame;
            m_mailboxState.hasPresented = true;
            didPresent = true;
        }

        bool didResize = false;
        if (m_resizePending.load(std::memory_order_relaxed) && m_mailboxState.hasPresented)
        {
            const bool noInFlight = (m_mailboxState.lastPresentedFrame + 1) == m_mailboxState.produceFrame;
            const bool workersDone = updateFinished >= m_mailboxState.lastPresentedFrame &&
                renderFinished >= m_mailboxState.lastPresentedFrame;
            if (noInFlight && workersDone)
            {
                apply_resize_for_next_frame(m_mailboxState.lastPresentedFrame + 1);
                fill_buffers(m_mailboxState.lastPresentedFrame + 1);
                m_resizePending.store(false, std::memory_order_relaxed);
                didResize = true;
            }
        }

        if (!didPresent && !didResize)
        {
            m_waiter.relax();
        }

        return true;
    }
    bool FramePipeline::step_backpressure()
    {
        // 1) Update/Render のキック条件を確認する
        // 2) 完了済みなら Present して進める
        if (!m_backpressureState.inFlight)
        {
            if (m_resizePending.load(std::memory_order_relaxed))
            {
                apply_resize_for_next_frame(m_backpressureState.currentFrame);
                fill_buffers(m_backpressureState.currentFrame);
                m_resizePending.store(false, std::memory_order_relaxed);
            }

            uint32_t updateIndex = 0;
            uint32_t renderIndex = 0;
            uint32_t presentIndex = 0;
            compute_indices(m_backpressureState.currentFrame, m_config.bufferCount,
                updateIndex, renderIndex, presentIndex);
            (void)presentIndex;

            m_updateJob.kick(m_backpressureState.currentFrame, updateIndex);
            m_renderJob.kick(m_backpressureState.currentFrame, renderIndex);
            m_backpressureState.inFlight = true;
        }

        const bool canPresent = m_updateJob.get_finished_frame() >= m_backpressureState.currentFrame &&
            m_renderJob.get_finished_frame() >= m_backpressureState.currentFrame;
        if (canPresent)
        {
            present_frame(m_backpressureState.currentFrame);
            ++m_backpressureState.currentFrame;
            m_backpressureState.inFlight = false;
        }
        else
        {
            m_waiter.relax();
        }

        return true;
    }
};

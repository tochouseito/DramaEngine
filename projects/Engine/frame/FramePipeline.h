#pragma once

// === C++ Standard Library ===
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>

// === Engine ===
#include "Core/Threading/IThread.h"
#include "Core/Threading/IThreadFactory.h"
#include "Core/Time/FrameCounter.h"

namespace Drama::Frame
{
    using UpdateFunc = std::function<void(uint64_t, uint32_t)>;
    using RenderFunc = std::function<void(uint64_t, uint32_t)>;
    using PresentFunc = std::function<void(uint64_t, uint32_t)>;

    enum class PipelineMode
    {
        Fixed,
        Mailbox,
        Backpressure,
    };

    struct FramePipelineDesc final
    {
        uint32_t m_bufferCount = 3;
        uint32_t m_maxFps = 60;
        PipelineMode m_mode = PipelineMode::Fixed;
    };

    class FrameJob final
    {
    public:
        using JobFunc = std::function<void(uint64_t, uint32_t)>;
        using ThreadFactory = Drama::Core::Threading::IThreadFactory;
        using Thread = Drama::Core::Threading::IThread;
        using StopToken = Drama::Core::Threading::StopToken;

        bool start(ThreadFactory& factory, const char* name, JobFunc func);

        void kick(uint64_t frameNo, uint32_t index);

        uint64_t get_finished_frame() const;

        void stop();

    private:
        struct Request final
        {
            uint64_t m_frameNo = 0;
            uint32_t m_index = 0;
        };

        static uint32_t thread_entry(StopToken token, void* user) noexcept;

        uint32_t thread_loop(StopToken token) noexcept;

        mutable std::mutex m_mutex;
        std::condition_variable m_cv;
        std::unique_ptr<Thread> m_thread;
        std::deque<Request> m_queue;
        JobFunc m_func;
        uint64_t m_finishedFrame = 0;
        bool m_exit = false;
    };

    class FramePipeline final
    {
        using Clock = Drama::Core::Time::Clock;
        using IWaiter = Drama::Core::Time::IWaiter;
        using ThreadFactory = Drama::Core::Threading::IThreadFactory;
    public:
        FramePipeline(const FramePipelineDesc& config, ThreadFactory& threadFactory, const Clock& clock, IWaiter& waiter,
            const UpdateFunc& updateFunc, const RenderFunc& renderFunc, const PresentFunc& presentFunc)
            : m_config(config)
            , m_threadFactory(threadFactory)
            , m_waiter(waiter)
            , m_frameCounter(clock, waiter)
            , m_updateFunc(updateFunc)
            , m_renderFunc(renderFunc)
            , m_presentFunc(presentFunc)
        {
            // 1) 初期化はメンバ初期化リストで完結させる
        }

        ~FramePipeline();

        void step();
        void poll_resize_request();

    private:
        struct FixedState final
        {
            uint64_t m_produceFrame = 0;
            uint64_t m_totalFrame = 0;
        };

        struct MailboxState final
        {
            uint64_t m_produceFrame = 0;
            uint64_t m_lastPresentedFrame = 0;
            bool m_hasPresented = false;
        };

        struct BackpressureState final
        {
            uint64_t m_currentFrame = 0;
            bool m_inFlight = false;
        };

        bool start_pipeline();
        void stop_jobs();

        void compute_indices(uint64_t frameNo, uint32_t bufferCount, uint32_t& updateIndex,
            uint32_t& renderIndex, uint32_t& presentIndex);

        void present_frame(uint64_t frameNo);

        void apply_resize_for_next_frame(uint64_t nextFrameNo);

        void fill_buffers(uint64_t frameNo);

        bool step_fixed();

        bool step_mailbox();

        bool step_backpressure();

        FramePipelineDesc m_config;
        Drama::Core::Threading::IThreadFactory& m_threadFactory;
        Drama::Core::Time::IWaiter& m_waiter;
        Drama::Core::Time::FrameCounter m_frameCounter;
        uint32_t m_backBufferBase = 0;
        std::atomic<bool> m_resizePending{ false };
        UpdateFunc m_updateFunc;
        RenderFunc m_renderFunc;
        PresentFunc m_presentFunc;
        FrameJob m_updateJob;
        FrameJob m_renderJob;
        FixedState m_fixedState{};
        MailboxState m_mailboxState{};
        BackpressureState m_backpressureState{};
        uint64_t m_maxLead = 0;
        bool m_started = false;
        bool m_finished = false;
    };
}

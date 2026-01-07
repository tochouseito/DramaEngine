#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <new>
#include <stdexcept>
#include <thread>
#include <vector>

#include "Core/Error/Result.h"
#include "Core/Threading/IThread.h"
#include "Core/Threading/IThreadFactory.h"
#include "Core/Threading/JobSystem.h"

namespace
{
    using namespace Drama::Core::Threading;
    using Drama::Core::Error::Result;

    struct TestCounters final
    {
        std::atomic<int> created{ 0 };
        std::atomic<int> started{ 0 };
        std::atomic<int> joined{ 0 };
        std::atomic<int> live{ 0 };
    };

    class TestThread final : public IThread
    {
    public:
        TestThread(TestCounters* counters, uint32_t threadId) noexcept
            : m_counters(counters)
            , m_threadId(threadId)
        {
        }

        ~TestThread() noexcept override
        {
            join_if_needed();
        }

        TestThread(const TestThread&) = delete;
        TestThread& operator=(const TestThread&) = delete;

        Result start(ThreadProc proc, void* user) noexcept
        {
            if (proc == nullptr)
            {
                return Result::fail(
                    Drama::Core::Error::Facility::Core,
                    Drama::Core::Error::Code::InvalidArg,
                    Drama::Core::Error::Severity::Error,
                    0,
                    "ThreadProc is null.");
            }

            try
            {
                m_thread = std::thread([this, proc, user]() noexcept
                    {
                        if (m_counters)
                        {
                            m_counters->started.fetch_add(1, std::memory_order_relaxed);
                            m_counters->live.fetch_add(1, std::memory_order_relaxed);
                        }

                        uint32_t code = 0;
                        try
                        {
                            code = proc(m_stopSource.token(), user);
                        }
                        catch (...)
                        {
                            code = 0xFFFFFFFFu;
                        }

                        m_exitCode.store(code, std::memory_order_relaxed);

                        if (m_counters)
                        {
                            m_counters->live.fetch_sub(1, std::memory_order_relaxed);
                        }
                    });
            }
            catch (...)
            {
                return Result::fail(
                    Drama::Core::Error::Facility::Core,
                    Drama::Core::Error::Code::Unknown,
                    Drama::Core::Error::Severity::Error,
                    0,
                    "std::thread creation failed.");
            }

            if (m_counters)
            {
                m_counters->created.fetch_add(1, std::memory_order_relaxed);
            }

            return Result::ok();
        }

        bool joinable() const noexcept override
        {
            return m_thread.joinable();
        }

        Result join() noexcept override
        {
            join_if_needed();
            return Result::ok();
        }

        void request_stop() noexcept override
        {
            m_stopSource.request_stop();
        }

        StopToken stop_token() const noexcept override
        {
            return m_stopSource.token();
        }

        uint32_t thread_id() const noexcept override
        {
            return m_threadId;
        }

        uint32_t exit_code() const noexcept override
        {
            return m_exitCode.load(std::memory_order_relaxed);
        }

    private:
        void join_if_needed() noexcept
        {
            if (m_thread.joinable())
            {
                request_stop();
                m_thread.join();
                if (m_counters)
                {
                    m_counters->joined.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }

        std::thread m_thread{};
        StopSource m_stopSource{};
        std::atomic<uint32_t> m_exitCode{ 0 };
        TestCounters* m_counters = nullptr;
        uint32_t m_threadId = 0;
    };

    class TestThreadFactory final : public IThreadFactory
    {
    public:
        explicit TestThreadFactory(TestCounters* counters, int failAfter = -1) noexcept
            : m_counters(counters)
            , m_failAfter(failAfter)
        {
        }

        Result create_thread(
            const ThreadDesc&,
            ThreadProc proc,
            void* user,
            std::unique_ptr<IThread>& outThread) noexcept override
        {
            const int callIndex = m_callCount.fetch_add(1, std::memory_order_relaxed) + 1;
            if (m_failAfter >= 0 && callIndex > m_failAfter)
            {
                return Result::fail(
                    Drama::Core::Error::Facility::Core,
                    Drama::Core::Error::Code::OutOfMemory,
                    Drama::Core::Error::Severity::Error,
                    0,
                    "Simulated thread creation failure.");
            }

            auto th = std::unique_ptr<TestThread>(new (std::nothrow) TestThread(
                m_counters, static_cast<uint32_t>(callIndex)));
            if (!th)
            {
                return Result::fail(
                    Drama::Core::Error::Facility::Core,
                    Drama::Core::Error::Code::OutOfMemory,
                    Drama::Core::Error::Severity::Error,
                    0,
                    "Failed to allocate TestThread.");
            }

            const auto r = th->start(proc, user);
            if (!r)
            {
                return r;
            }

            outThread = std::move(th);
            return Result::ok();
        }

    private:
        TestCounters* m_counters = nullptr;
        int m_failAfter = -1;
        std::atomic<int> m_callCount{ 0 };
    };

    int g_failures = 0;

    void report_failure(const char* expr, const char* file, int line) noexcept
    {
        ++g_failures;
        std::cerr << file << ":" << line << " FAIL: " << expr << "\n";
    }

    bool wait_until_eq(
        const std::atomic<int>& value,
        int target,
        std::chrono::milliseconds timeout) noexcept
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline)
        {
            if (value.load(std::memory_order_relaxed) == target)
            {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return value.load(std::memory_order_relaxed) == target;
    }

#define EXPECT_TRUE(expr) do { if (!(expr)) { report_failure(#expr, __FILE__, __LINE__); } } while (0)
#define EXPECT_FALSE(expr) EXPECT_TRUE(!(expr))
#define EXPECT_EQ(a, b) do { auto _a = (a); auto _b = (b); if (!(_a == _b)) { report_failure(#a " == " #b, __FILE__, __LINE__); } } while (0)

    void test_job_exception_propagates()
    {
        TestCounters counters{};
        TestThreadFactory factory(&counters);

        JobSystem js{};
        const auto r = js.initialize(factory, 1, 1, 32, 32);
        EXPECT_TRUE(r);

        std::shared_future<void> f{};
        const auto r2 = js.enqueue_job(
            "throws",
            []()
            {
                throw std::runtime_error("boom");
            },
            f);
        EXPECT_TRUE(r2);
        EXPECT_TRUE(f.valid());

        js.wait_for_job(f);

        bool threw = false;
        try
        {
            f.get();
        }
        catch (...)
        {
            threw = true;
        }
        EXPECT_TRUE(threw);

        js.shutdown();
    }

    void test_clear_queued_jobs_releases_wait()
    {
        TestCounters counters{};
        TestThreadFactory factory(&counters);

        JobSystem js{};
        const auto r = js.initialize(factory, 1, 1, 64, 64);
        EXPECT_TRUE(r);

        std::promise<void> gate{};
        const auto gateFuture = gate.get_future().share();

        constexpr int k_jobs = 8;
        for (int i = 0; i < k_jobs; ++i)
        {
            std::shared_future<void> f{};
            const auto r2 = js.enqueue_job(
                "blocked",
                []() {},
                f,
                JobSystem::JobPriority::Normal,
                std::vector<std::shared_future<void>>{ gateFuture });
            EXPECT_TRUE(r2);
        }

        auto waitFuture = std::async(std::launch::async, [&js]()
            {
                js.wait_for_all();
                return true;
            });

        js.clear_queued_jobs();

        const auto st = waitFuture.wait_for(std::chrono::milliseconds(500));
        EXPECT_TRUE(st == std::future_status::ready);
        if (st == std::future_status::ready)
        {
            (void)waitFuture.get();
        }
        else
        {
            js.shutdown();
            (void)waitFuture.wait();
        }

        EXPECT_EQ(js.queued_job_count(), static_cast<std::size_t>(0));

        js.shutdown();
    }

    void test_init_failure_cleans_threads()
    {
        TestCounters counters{};
        TestThreadFactory factory(&counters, 2);

        JobSystem js{};
        const auto r = js.initialize(factory, 4, 4, 32, 32);
        EXPECT_FALSE(r);

        const bool stopped = wait_until_eq(counters.live, 0, std::chrono::milliseconds(500));
        EXPECT_TRUE(stopped);
        EXPECT_EQ(counters.joined.load(std::memory_order_relaxed),
            counters.created.load(std::memory_order_relaxed));
    }
}

int main()
{
    test_job_exception_propagates();
    test_clear_queued_jobs_releases_wait();
    test_init_failure_cleans_threads();

    if (g_failures != 0)
    {
        std::cerr << "FAILED (" << g_failures << ")\n";
        return 1;
    }

    std::cout << "OK\n";
    return 0;
}

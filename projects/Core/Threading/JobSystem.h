#pragma once

// === C++ Standard Library ===
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// === Engine ===
#include "Core/Error/Result.h"
#include "Core/Threading/IThread.h"
#include "Core/Threading/IThreadFactory.h"

namespace Drama::Core::Threading
{
    class JobSystem final
    {
    public:
        enum class JobPriority : uint8_t
        {
            High = 0,
            Normal = 1,
            Low = 2,
        };

        struct Job final
        {
            std::string name{};
            std::function<void()> func{};
            JobPriority priority = JobPriority::Normal;
            std::vector<std::shared_future<void>> dependencies{};
            std::shared_ptr<std::promise<void>> promise{};
        };

        JobSystem() noexcept = default;
        ~JobSystem() noexcept
        {
            // 1) 破棄時に取り残しを避ける
            shutdown();
        }

        JobSystem(const JobSystem&) = delete;
        JobSystem& operator=(const JobSystem&) = delete;

        Core::Error::Result initialize(
            IThreadFactory& factory,
            uint32_t requestedWorkerCount = 0,
            uint32_t maxWorkerCount = 64) noexcept
        {
            // 1) 二重初期化防止
            if (m_isInitialized.load(std::memory_order_relaxed))
            {
                return Core::Error::Result::ok();
            }

            // 2) ワーカー数決定（0なら自動）
            uint32_t workerCount = requestedWorkerCount;
            if (workerCount == 0)
            {
                const uint32_t hc = std::thread::hardware_concurrency();
                workerCount = (hc != 0) ? hc : 4;
            }
            if (workerCount > maxWorkerCount)
            {
                workerCount = maxWorkerCount;
            }
            if (workerCount == 0)
            {
                workerCount = 1;
            }

            // 3) 状態初期化
            m_stopRequested.store(false, std::memory_order_relaxed);
            m_inFlight.store(0, std::memory_order_relaxed);

            // 4) スレッド確保
            m_workers.clear();
            m_workers.resize(workerCount);

            // 5) ワーカースレッド生成
            for (uint32_t i = 0; i < workerCount; ++i)
            {
                // 1) name の実体（std::string）を保持する
                const std::string workerName = make_worker_name(i);

                // 2) desc を組み立てる（string_view は workerName を参照）
                ThreadDesc desc{};
                desc.name = workerName;
                desc.stackSizeBytes = 0;
                desc.priority = 0;
                desc.affinityMask = 0;

                // 3) スレッド生成（create_thread が戻るまで workerName は生存している）
                std::unique_ptr<IThread> th{};
                const auto r = factory.create_thread(desc, &JobSystem::worker_entry, this, th);
                if (!r)
                {
                    shutdown();
                    return r;
                }

                m_workers[i] = std::move(th);
            }

            m_isInitialized.store(true, std::memory_order_relaxed);
            return Core::Error::Result::ok();
        }

        void shutdown() noexcept
        {
            // 1) 初期化されていなければ何もしない
            if (!m_isInitialized.load(std::memory_order_relaxed))
            {
                return;
            }

            // 2) 停止フラグを立てる
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_stopRequested.store(true, std::memory_order_relaxed);
            }

            // 3) 待機中ワーカーを起こす
            m_cv.notify_all();

            // 4) スレッド側の stop も要求（StopToken 経由）
            for (auto& th : m_workers)
            {
                if (th)
                {
                    th->request_stop();
                }
            }

            // 5) join
            for (auto& th : m_workers)
            {
                if (th)
                {
                    (void)th->join();
                    th.reset();
                }
            }

            // 6) キューを掃除
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_high.clear();
                m_normal.clear();
                m_low.clear();
            }

            m_isInitialized.store(false, std::memory_order_relaxed);
        }

        Core::Error::Result enqueue_job(
            std::string jobName,
            std::function<void()> job,
            std::shared_future<void>& outFuture,
            JobPriority priority = JobPriority::Normal,
            const std::vector<std::shared_future<void>>& dependencies = {}) noexcept
        {
            // 1) 初期化チェック
            if (!m_isInitialized.load(std::memory_order_relaxed))
            {
                return Core::Error::Result::fail(
                    Core::Error::Facility::Core,
                    Core::Error::Code::InvalidState,
                    Core::Error::Severity::Error,
                    0,
                    "JobSystem is not initialized.");
            }

            // 2) promise/future 作成
            auto promise = std::make_shared<std::promise<void>>();
            outFuture = promise->get_future().share();

            // 3) Job 構築
            Job j{};
            j.name = std::move(jobName);
            j.func = std::move(job);
            j.priority = priority;
            j.dependencies = dependencies;
            j.promise = std::move(promise);

            // 4) キューに積む（inFlightも増やす）
            {
                std::lock_guard<std::mutex> lock(m_mutex);

                if (m_stopRequested.load(std::memory_order_relaxed))
                {
                    return Core::Error::Result::fail(
                        Core::Error::Facility::Core,
                        Core::Error::Code::InvalidState,
                        Core::Error::Severity::Error,
                        0,
                        "JobSystem is stopping.");
                }

                push_job_no_lock(std::move(j));
                m_inFlight.fetch_add(1, std::memory_order_relaxed);
            }

            // 5) ワーカーを起こす
            m_cv.notify_one();
            return Core::Error::Result::ok();
        }

        Core::Error::Result enqueue_batch_job(
            const std::string& batchName,
            std::vector<std::function<void()>> jobs,
            std::shared_future<void>& outFuture,
            JobPriority priority = JobPriority::Normal) noexcept
        {
            // 1) バッチは1ジョブとして登録
            return enqueue_job(
                batchName,
                [jobs = std::move(jobs)]()
                {
                    // 1) 逐次実行（必要なら将来ここを並列化）
                    for (const auto& f : jobs)
                    {
                        f();
                    }
                },
                outFuture,
                priority);
        }

        void wait_for_job(const std::shared_future<void>& job) noexcept
        {
            // 1) validなら待つ
            if (job.valid())
            {
                job.wait();
            }
        }

        void wait_for_all() noexcept
        {
            // 1) inFlight が 0 になるまで待つ（完了時に notify_all している）
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]()
                {
                    return (m_inFlight.load(std::memory_order_relaxed) == 0);
                });
        }

        std::size_t queued_job_count() const noexcept
        {
            // 1) キューサイズ合算
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_high.size() + m_normal.size() + m_low.size();
        }

        std::size_t worker_count() const noexcept
        {
            // 1) ワーカー数
            return m_workers.size();
        }

        void clear_queued_jobs() noexcept
        {
            // 1) 実行待ちだけ消す（inFlightは整合しないので通常はshutdown時のみ推奨）
            std::lock_guard<std::mutex> lock(m_mutex);
            m_high.clear();
            m_normal.clear();
            m_low.clear();
        }

    private:
        static uint32_t worker_entry(StopToken token, void* user) noexcept
        {
            // 1) this を取得
            auto* self = static_cast<JobSystem*>(user);
            if (!self)
            {
                return 0;
            }

            // 2) ループ実行
            return self->worker_loop(token);
        }

        uint32_t worker_loop(StopToken token) noexcept
        {
            while (!token.stop_requested())
            {
                Job job{};
                bool hasJob = false;

                // 1) ジョブ取得（無ければ待つ）
                {
                    std::unique_lock<std::mutex> lock(m_mutex);

                    while (!token.stop_requested())
                    {
                        if (m_stopRequested.load(std::memory_order_relaxed))
                        {
                            return 0;
                        }

                        hasJob = try_pop_executable_job_no_lock(job);
                        if (hasJob)
                        {
                            break;
                        }

                        // 2) 実行可能が無いなら待つ（enqueue/完了で起きる）
                        m_cv.wait(lock);
                    }
                }

                if (!hasJob)
                {
                    continue;
                }

                // 3) 実行
                job.func();

                // 4) 完了通知
                job.promise->set_value();

                // 5) inFlight 減算して全員起こす（依存が解ける可能性がある）
                m_inFlight.fetch_sub(1, std::memory_order_relaxed);
                m_cv.notify_all();
            }

            return 0;
        }

        void push_job_no_lock(Job&& j) noexcept
        {
            // 1) 優先度別に格納
            switch (j.priority)
            {
            case JobPriority::High:
            {
                m_high.push_back(std::move(j));
                break;
            }
            case JobPriority::Normal:
            {
                m_normal.push_back(std::move(j));
                break;
            }
            case JobPriority::Low:
            default:
            {
                m_low.push_back(std::move(j));
                break;
            }
            }
        }

        bool try_pop_executable_job_no_lock(Job& outJob) noexcept
        {
            // 1) 高→通常→低 の順で探す
            if (try_pop_from_queue_no_lock(m_high, outJob))
            {
                return true;
            }
            if (try_pop_from_queue_no_lock(m_normal, outJob))
            {
                return true;
            }
            if (try_pop_from_queue_no_lock(m_low, outJob))
            {
                return true;
            }

            return false;
        }

        bool try_pop_from_queue_no_lock(std::vector<Job>& q, Job& outJob) noexcept
        {
            // 1) 実行可能なものを線形探索
            for (std::size_t i = 0; i < q.size(); ++i)
            {
                if (can_execute_job_no_lock(q[i]))
                {
                    // 2) 取り出し（swap-pop）
                    outJob = std::move(q[i]);
                    q[i] = std::move(q.back());
                    q.pop_back();
                    return true;
                }
            }

            return false;
        }

        bool can_execute_job_no_lock(const Job& job) const noexcept
        {
            // 1) 依存が無ければ即実行
            if (job.dependencies.empty())
            {
                return true;
            }

            // 2) 依存futureが ready か確認
            for (const auto& dep : job.dependencies)
            {
                if (dep.valid())
                {
                    const auto st = dep.wait_for(std::chrono::seconds(0));
                    if (st != std::future_status::ready)
                    {
                        return false;
                    }
                }
            }

            return true;
        }

        static std::string make_worker_name(uint32_t index)
        {
            // 1) デバッグ用（初期化時だけなので簡易でOK）
            return "JobWorker_" + std::to_string(index);
        }

    private:
        mutable std::mutex m_mutex{};
        std::condition_variable m_cv{};

        std::atomic<bool> m_isInitialized{ false };
        std::atomic<bool> m_stopRequested{ false };
        std::atomic<uint32_t> m_inFlight{ 0 };

        std::vector<std::unique_ptr<IThread>> m_workers{};

        // 1) 優先度別キュー（swap-popで順序は崩れる。必要ならdeque/FIFO化）
        std::vector<Job> m_high{};
        std::vector<Job> m_normal{};
        std::vector<Job> m_low{};
    };
}

#pragma once

// === C++ Standard Library ===
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

// === Engine ===
#include "Core/Threading/IThread.h"
#include "Core/Threading/IThreadFactory.h"
#include "Core/Memory/FixedBlockPool.h"
#include "Core/Error/Result.h"

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

            // NOTE:
            //  - 共有ポインタをやめて、Job が “その場” に居続ける前提で promise を値で持つ
            //  - キューが Job* を持つので、Job の move/copy を原則ゼロにできる
            std::promise<void> promise{};
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
            uint32_t maxWorkerCount = 64,
            uint32_t maxJobsInFlight = 4096) noexcept
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

            // 3) ジョブプール初期化（ここで “置き場” を確保して、以降の enqueue でのヒープ確保を減らす）
            {
                const Core::Error::Result pr = pool_init(m_jobPool, maxJobsInFlight);
                if (!pr)
                {
                    return pr;
                }
            }

            // 4) 状態初期化
            m_stopRequested.store(false, std::memory_order_relaxed);
            m_inFlight.store(0, std::memory_order_relaxed);

            // 5) キュー/スレッド確保
            m_high.clear();
            m_normal.clear();
            m_low.clear();

            m_workers.clear();
            m_workers.resize(workerCount);

            // 6) ワーカースレッド生成
            for (uint32_t i = 0; i < workerCount; ++i)
            {
                const std::string workerName = make_worker_name(i);

                ThreadDesc desc{};
                desc.name = workerName;
                desc.stackSizeBytes = 0;
                desc.priority = 0;
                desc.affinityMask = 0;

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

            // 4) スレッド側にも停止要求
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

            // 6) キューに残ったジョブを回収（promise は破棄されるので wait は解除される）
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                drain_queue_no_lock(m_high);
                drain_queue_no_lock(m_normal);
                drain_queue_no_lock(m_low);

                // join 済みなので inFlight は強制的に 0 に戻す
                m_inFlight.store(0, std::memory_order_relaxed);
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

            // 2) Job をプールから確保（キューは Job* を持つ）
            Job* j = nullptr;
            {
                const Core::Error::Result ar = pool_alloc(m_jobPool, j);
                if (!ar)
                {
                    return ar;
                }
            }

            // 3) Job を組み立てる（ここで promise/future を作る）
            //    NOTE: promise は “値” で持つので make_shared を排除できる
            j->name = std::move(jobName);
            j->func = std::move(job);
            j->priority = priority;
            j->dependencies = dependencies;

            j->promise = std::promise<void>{};
            outFuture = j->promise.get_future().share();

            // 4) キューに積む（停止中なら回収してエラー）
            {
                std::lock_guard<std::mutex> lock(m_mutex);

                if (m_stopRequested.load(std::memory_order_relaxed))
                {
                    // 1) 組み立て済み Job を戻す
                    reset_job_fields(*j);
                    pool_free(m_jobPool, j);

                    return Core::Error::Result::fail(
                        Core::Error::Facility::Core,
                        Core::Error::Code::InvalidState,
                        Core::Error::Severity::Error,
                        0,
                        "JobSystem is stopping.");
                }

                push_job_no_lock(j);
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
            // 1) inFlight が 0 になるまで待つ
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
            // 1) 実行待ちだけ消す（inFlight は整合しないので shutdown 時のみ推奨）
            std::lock_guard<std::mutex> lock(m_mutex);
            drain_queue_no_lock(m_high);
            drain_queue_no_lock(m_normal);
            drain_queue_no_lock(m_low);
        }

    private:
        // ---------- FixedBlockPool API 吸収 ----------
        template<class Pool>
        static Core::Error::Result pool_init(Pool& pool, uint32_t capacity) noexcept
        {
            // 1) よくある候補を順に試す（存在するものだけコンパイルされる）
            if constexpr (requires(Pool p, uint32_t c) { p.initialize(c); })
            {
                return pool.initialize(capacity);
            }
            else if constexpr (requires(Pool p, uint32_t c) { p.init(c); })
            {
                return pool.init(capacity);
            }
            else if constexpr (requires(Pool p, uint32_t c) { p.create(c); })
            {
                return pool.create(capacity);
            }
            else
            {
                return Core::Error::Result::fail(
                    Core::Error::Facility::Core,
                    Core::Error::Code::Unsupported,
                    Core::Error::Severity::Error,
                    0,
                    "FixedBlockPool: init method not found.");
            }
        }

        template<class Pool, class T>
        static Core::Error::Result pool_alloc(Pool& pool, T*& outPtr) noexcept
        {
            // 1) allocate / alloc / try_allocate を吸収
            if constexpr (requires(Pool p, T * &o) { p.allocate(o); })
            {
                return pool.allocate(outPtr);
            }
            else if constexpr (requires(Pool p, T * &o) { p.alloc(o); })
            {
                return pool.alloc(outPtr);
            }
            else if constexpr (requires(Pool p, T * &o) { p.try_allocate(o); })
            {
                return pool.try_allocate(outPtr);
            }
            else
            {
                return Core::Error::Result::fail(
                    Core::Error::Facility::Core,
                    Core::Error::Code::Unsupported,
                    Core::Error::Severity::Error,
                    0,
                    "FixedBlockPool: alloc method not found.");
            }
        }

        template<class Pool, class T>
        static void pool_free(Pool& pool, T* ptr) noexcept
        {
            // 1) free / deallocate / release を吸収
            if constexpr (requires(Pool p, T * x) { p.free(x); })
            {
                pool.free(ptr);
            }
            else if constexpr (requires(Pool p, T * x) { p.deallocate(x); })
            {
                pool.deallocate(ptr);
            }
            else if constexpr (requires(Pool p, T * x) { p.release(x); })
            {
                pool.release(ptr);
            }
            else
            {
                // 2) ここに来る設計はダメ（静かにリークする）
                //    API 名を合わせろ
                (void)pool;
                (void)ptr;
            }
        }

        // ---------- Worker ----------
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
                Job* job = nullptr;

                // 1) ジョブ取得（無ければ待つ）
                {
                    std::unique_lock<std::mutex> lock(m_mutex);

                    while (!token.stop_requested())
                    {
                        if (m_stopRequested.load(std::memory_order_relaxed))
                        {
                            return 0;
                        }

                        if (try_pop_executable_job_no_lock(job))
                        {
                            break;
                        }

                        // 2) 実行可能が無いなら待つ（enqueue/完了で起きる）
                        m_cv.wait(lock);

                        // 3) 起床直後に軽く譲る（過剰な奪い合いを減らす）
                        std::this_thread::yield();
                    }
                }

                if (!job)
                {
                    continue;
                }

                // 2) 実行
                job->func();

                // 3) 完了通知
                job->promise.set_value();

                // 4) ジョブ回収（次回再利用できるようにフィールドを掃除）
                reset_job_fields(*job);
                pool_free(m_jobPool, job);

                // 5) inFlight 減算して通知（依存が解ける可能性がある）
                m_inFlight.fetch_sub(1, std::memory_order_relaxed);
                m_cv.notify_all();
            }

            return 0;
        }

        // ---------- Queue ----------
        void push_job_no_lock(Job* j) noexcept
        {
            // 1) 優先度別に格納
            switch (j->priority)
            {
            case JobPriority::High:
            {
                m_high.push_back(j);
                break;
            }
            case JobPriority::Normal:
            {
                m_normal.push_back(j);
                break;
            }
            case JobPriority::Low:
            default:
            {
                m_low.push_back(j);
                break;
            }
            }
        }

        bool try_pop_executable_job_no_lock(Job*& outJob) noexcept
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

        bool try_pop_from_queue_no_lock(std::vector<Job*>& q, Job*& outJob) noexcept
        {
            // 1) 実行可能なものを線形探索
            for (std::size_t i = 0; i < q.size(); ++i)
            {
                Job* cand = q[i];
                if (cand && can_execute_job_no_lock(*cand))
                {
                    // 2) 取り出し（swap-pop）
                    outJob = cand;
                    q[i] = q.back();
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

        void drain_queue_no_lock(std::vector<Job*>& q) noexcept
        {
            // 1) 残ジョブをすべて回収
            for (Job* j : q)
            {
                if (j)
                {
                    reset_job_fields(*j);
                    pool_free(m_jobPool, j);
                }
            }
            q.clear();
        }

        static void reset_job_fields(Job& j) noexcept
        {
            // 1) 再利用のために “重い要素” を空にする（capacity は残るので再利用が効く）
            j.name.clear();
            j.func = {};
            j.dependencies.clear();

            // 2) promise は次 enqueue で作り直す
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

        // 1) 優先度別キュー（ポインタだけ保持）
        std::vector<Job*> m_high{};
        std::vector<Job*> m_normal{};
        std::vector<Job*> m_low{};

        // 2) Job の置き場（FixedBlockPool）
        Core::Memory::FixedBlockPool<Job> m_jobPool{};
    };
}

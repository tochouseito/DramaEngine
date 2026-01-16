#pragma once
#include "stdafx.h"
#include <mutex>
#include <queue>

namespace Drama::Graphics::DX12
{
    class RenderDevice;

    class CommandContext
    {
    public:
        CommandContext(RenderDevice& renderDevice, D3D12_COMMAND_LIST_TYPE type);
        virtual ~CommandContext() = default;
        void reset();
        void close();
        ID3D12GraphicsCommandList* get_command_list() { return m_commandList.Get(); }
        ID3D12GraphicsCommandList1* get_command_list1()
        {
            ID3D12GraphicsCommandList1* commandList1 = nullptr;
            m_commandList->QueryInterface(IID_PPV_ARGS(&commandList1));
            return commandList1;
        }
        ID3D12GraphicsCommandList2* get_command_list2()
        {
            ID3D12GraphicsCommandList2* commandList2 = nullptr;
            m_commandList->QueryInterface(IID_PPV_ARGS(&commandList2));
            return commandList2;
        }
        ID3D12GraphicsCommandList3* get_command_list3()
        {
            ID3D12GraphicsCommandList3* commandList3 = nullptr;
            m_commandList->QueryInterface(IID_PPV_ARGS(&commandList3));
            return commandList3;
        }
        ID3D12GraphicsCommandList4* get_command_list4()
        {
            ID3D12GraphicsCommandList4* commandList4 = nullptr;
            m_commandList->QueryInterface(IID_PPV_ARGS(&commandList4));
            return commandList4;
        }
        ID3D12GraphicsCommandList5* get_command_list5()
        {
            ID3D12GraphicsCommandList5* commandList5 = nullptr;
            m_commandList->QueryInterface(IID_PPV_ARGS(&commandList5));
            return commandList5;
        }
        ID3D12GraphicsCommandList6* get_command_list6()
        {
            ID3D12GraphicsCommandList6* commandList6 = nullptr;
            m_commandList->QueryInterface(IID_PPV_ARGS(&commandList6));
            return commandList6;
        }
        ID3D12GraphicsCommandList7* get_command_list7()
        {
            ID3D12GraphicsCommandList7* commandList7 = nullptr;
            m_commandList->QueryInterface(IID_PPV_ARGS(&commandList7));
            return commandList7;
        }
        ID3D12GraphicsCommandList8* get_command_list8()
        {
            ID3D12GraphicsCommandList8* commandList8 = nullptr;
            m_commandList->QueryInterface(IID_PPV_ARGS(&commandList8));
            return commandList8;
        }
        ID3D12GraphicsCommandList9* get_command_list9()
        {
            ID3D12GraphicsCommandList9* commandList9 = nullptr;
            m_commandList->QueryInterface(IID_PPV_ARGS(&commandList9));
            return commandList9;
        }
        ID3D12GraphicsCommandList10* get_command_list10()
        {
            ID3D12GraphicsCommandList10* commandList10 = nullptr;
            m_commandList->QueryInterface(IID_PPV_ARGS(&commandList10));
            return commandList10;
        }
        ID3D12CommandAllocator* get_command_allocator() { return m_commandAllocator.Get(); }

        /*===== Commands =====*/
    protected:
        RenderDevice& m_renderDevice;
        ComPtr<ID3D12GraphicsCommandList> m_commandList = nullptr;
        ComPtr<ID3D12CommandAllocator> m_commandAllocator = nullptr;
    };

    class GraphicsCommandContext : public CommandContext
    {
    public:
        GraphicsCommandContext(RenderDevice& renderDevice)
            :CommandContext(renderDevice, D3D12_COMMAND_LIST_TYPE_DIRECT)
        {
        }
        ~GraphicsCommandContext() = default;
    };

    class ComputeCommandContext : public CommandContext
    {
        public:
        ComputeCommandContext(RenderDevice& renderDevice)
            :CommandContext(renderDevice, D3D12_COMMAND_LIST_TYPE_COMPUTE)
        {
        }
        ~ComputeCommandContext() = default;
    };

    class CopyCommandContext : public CommandContext
    {
        public:
        CopyCommandContext(RenderDevice& renderDevice)
            :CommandContext(renderDevice, D3D12_COMMAND_LIST_TYPE_COPY)
        {
        }
        ~CopyCommandContext() = default;
    };

    class CommandPool final
    {
    public:
        /// @brief コンストラクタ
        CommandPool(RenderDevice& device)
            : m_renderDevice(device)
        {
        }
        /// @brief デストラクタ
        ~CommandPool() = default;

        GraphicsCommandContext* get_graphics_context()
        {
            std::lock_guard<std::mutex> lock(m_graphicsMutex);
            if (m_graphicsCtxPool.empty())
            {
                auto context = std::make_unique<GraphicsCommandContext>(m_renderDevice);
                return context.release();
            }
            auto context = std::move(m_graphicsCtxPool.front());
            m_graphicsCtxPool.pop();
            return context.release();
        }

        ComputeCommandContext* get_compute_context()
        {
            std::lock_guard<std::mutex> lock(m_computeMutex);
            if (m_computeCtxPool.empty())
            {
                auto context = std::make_unique<ComputeCommandContext>(m_renderDevice);
                return context.release();
            }
            auto context = std::move(m_computeCtxPool.front());
            m_computeCtxPool.pop();
            return context.release();
        }

        CopyCommandContext* get_copy_context()
        {
            std::lock_guard<std::mutex> lock(m_copyMutex);
            if (m_copyCtxPool.empty())
            {
                auto context = std::make_unique<CopyCommandContext>(m_renderDevice);
                return context.release();
            }
            auto context = std::move(m_copyCtxPool.front());
            m_copyCtxPool.pop();
            return context.release();
        }

        void return_context(GraphicsCommandContext* context)
        {
            std::lock_guard<std::mutex> lock(m_graphicsMutex);
            m_graphicsCtxPool.push(std::unique_ptr<GraphicsCommandContext>(context));
        }

        void return_context(ComputeCommandContext* context)
        {
            std::lock_guard<std::mutex> lock(m_computeMutex);
            m_computeCtxPool.push(std::unique_ptr<ComputeCommandContext>(context));
        }

        void return_context(CopyCommandContext* context)
        {
            std::lock_guard<std::mutex> lock(m_copyMutex);
            m_copyCtxPool.push(std::unique_ptr<CopyCommandContext>(context));
        }
    private:
        RenderDevice& m_renderDevice;

        std::mutex m_graphicsMutex;
        std::queue<std::unique_ptr<GraphicsCommandContext>> m_graphicsCtxPool;
        std::mutex m_computeMutex;
        std::queue<std::unique_ptr<ComputeCommandContext>> m_computeCtxPool;
        std::mutex m_copyMutex;
        std::queue<std::unique_ptr<CopyCommandContext>> m_copyCtxPool;
    };

    enum class QueueType : uint8_t
    {
        Graphics,
        Compute,
        Copy,
        Count
    };

    class QueueContext
    {
    public:
        /// @brief コンストラクタ
        QueueContext(RenderDevice& device, D3D12_COMMAND_LIST_TYPE type);
        /// @brief デストラクタ
        virtual ~QueueContext()
        {
            flush();
            if (m_fenceEvent)
            {
                CloseHandle(m_fenceEvent);
                m_fenceEvent = nullptr;
            }
            m_fence.Reset();
            m_commandQueue.Reset();
        }

        void execute(CommandContext* ctx)
        {
            std::lock_guard<std::mutex> lock(m_fenceMutex);
            if (ctx)
            {
                ID3D12CommandList* lists[] = { ctx->get_command_list()};
                m_commandQueue->ExecuteCommandLists(1, lists);
            }
            m_fenceValue++;
            // GPUがここまでたどり着いたときに、Fenceの値を指定した値に代入するようにSignalを送る
            m_commandQueue->Signal(m_fence.Get(), m_fenceValue);
        }
        void flush()
        {
            const UINT64 fence = ++m_fenceValue;
            m_commandQueue->Signal(m_fence.Get(), fence);

            if (m_fence->GetCompletedValue() < fence)
            {
                m_fence->SetEventOnCompletion(fence, m_fenceEvent);
                WaitForSingleObject(m_fenceEvent, INFINITE);
            }
        }
        void wait_fence()
        {
            // Fenceの値が指定したSignal値にたどり着いているか確認する
            // GetCompletedValueの初期値はFence作成時に渡した初期値
            if (!m_fenceValue) { return; }
            if (m_fence->GetCompletedValue() < m_fenceValue)
            {
                // 指定したSignalにたどり着いていないので、たどり着くまで待つようにイベントを設定する
                m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
                // イベント待つ
                WaitForSingleObject(m_fenceEvent, INFINITE);
            }
        }

        ID3D12CommandQueue* get_command_queue() noexcept { return m_commandQueue.Get(); };
        ID3D12Fence* get_fence() noexcept { return m_fence.Get(); };
        uint64_t get_fence_value() noexcept { return m_fenceValue; };
    private:
        ComPtr<ID3D12CommandQueue> m_commandQueue = nullptr;
        ComPtr<ID3D12Fence> m_fence = nullptr;
        HANDLE m_fenceEvent = {};
        uint64_t m_fenceValue = {};
        std::mutex m_fenceMutex;
        std::condition_variable m_fenceCV;
    };

    class GraphicsQueueContext final : public QueueContext
    {
    public:
        /// @brief コンストラクタ
        GraphicsQueueContext(RenderDevice& device)
            : QueueContext(device, D3D12_COMMAND_LIST_TYPE_DIRECT)
        {
        }
        /// @brief デストラクタ
        ~GraphicsQueueContext() = default;
    };

    class ComputeQueueContext final : public QueueContext
    {
    public:
        /// @brief コンストラクタ
        ComputeQueueContext(RenderDevice& device)
            : QueueContext(device, D3D12_COMMAND_LIST_TYPE_COMPUTE)
        {
        }
        /// @brief デストラクタ
        ~ComputeQueueContext() = default;
    };

    class CopyQueueContext final : public QueueContext
    {
    public:
        /// @brief コンストラクタ
        CopyQueueContext(RenderDevice& device)
            : QueueContext(device, D3D12_COMMAND_LIST_TYPE_COPY)
        {
        }
        /// @brief デストラクタ
        ~CopyQueueContext() = default;
    };

    class QueuePool final
    {
    public:
        /// @brief コンストラクタ
        QueuePool(RenderDevice& device)
            : m_renderDevice(device)
        {
            // グラフィックスキューのプールを初期化
            for (uint32_t i = 0; i < k_graphicsQueueCount; ++i)
            {
                m_graphicsQueuePool.push(std::make_unique<GraphicsQueueContext>(m_renderDevice));
            }
            // コンピュートキューのプールを初期化
            for (uint32_t i = 0; i < k_computeQueueCount; ++i)
            {
                m_computeQueuePool.push(std::make_unique<ComputeQueueContext>(m_renderDevice));
            }
            // コピーキューのプールを初期化
            for (uint32_t i = 0; i < k_copyQueueCount; ++i)
            {
                m_copyQueuePool.push(std::make_unique<CopyQueueContext>(m_renderDevice));
            }
            // Present用キューを初期化
            m_presentQueue = std::make_unique<GraphicsQueueContext>(m_renderDevice);
        }
        /// @brief デストラクタ
        ~QueuePool()
        {
            flush_all();
        }
        GraphicsQueueContext* get_graphics_queue()
        {
            std::unique_lock<std::mutex> lock(m_graphicsMutex);
            m_graphicsCV.wait(lock, [this]() { return !m_graphicsQueuePool.empty(); });
            auto queue = std::move(m_graphicsQueuePool.front());
            m_graphicsQueuePool.pop();
            return queue.release();
        }
        ComputeQueueContext* get_compute_queue()
        {
            std::unique_lock<std::mutex> lock(m_computeMutex);
            m_computeCV.wait(lock, [this]() { return !m_computeQueuePool.empty(); });
            auto queue = std::move(m_computeQueuePool.front());
            m_computeQueuePool.pop();
            return queue.release();
        }
        CopyQueueContext* get_copy_queue()
        {
            std::unique_lock<std::mutex> lock(m_copyMutex);
            m_copyCV.wait(lock, [this]() { return !m_copyQueuePool.empty(); });
            auto queue = std::move(m_copyQueuePool.front());
            m_copyQueuePool.pop();
            return queue.release();
        }

        GraphicsQueueContext* get_present_queue()
        {
            return m_presentQueue.get();
        }

        void return_queue(GraphicsQueueContext* queue)
        {
            {
                std::lock_guard<std::mutex> lock(m_graphicsMutex);
                m_graphicsQueuePool.push(std::unique_ptr<GraphicsQueueContext>(queue));
            }
            m_graphicsCV.notify_one();
        }

        void return_queue(ComputeQueueContext* queue)
        {
            {
                std::lock_guard<std::mutex> lock(m_computeMutex);
                m_computeQueuePool.push(std::unique_ptr<ComputeQueueContext>(queue));
            }
            m_computeCV.notify_one();
        }

        void return_queue(CopyQueueContext* queue)
        {
            {
                std::lock_guard<std::mutex> lock(m_copyMutex);
                m_copyQueuePool.push(std::unique_ptr<CopyQueueContext>(queue));
            }
            m_copyCV.notify_one();
        }

        // 全キューのFlush
        void flush_all()
        {
            // queue を破棄せずに一周しながら Flush するヘルパ
            auto flush_queue_pool = [](std::mutex& mutex, auto& pool)
                {
                    std::lock_guard<std::mutex> lock(mutex);

                    const size_t count = pool.size();
                    for (size_t i = 0; i < count; ++i)
                    {
                        // 先頭を取り出して Flush してから末尾に戻す
                        auto queue = std::move(pool.front());
                        pool.pop();

                        if (queue)
                        {
                            queue->flush();
                        }

                        pool.push(std::move(queue));
                    }
                };

            flush_queue_pool(m_graphicsMutex, m_graphicsQueuePool);
            flush_queue_pool(m_computeMutex, m_computeQueuePool);
            flush_queue_pool(m_copyMutex, m_copyQueuePool);
            // present キューも Flush
            if (m_presentQueue)
            {
                m_presentQueue->flush();
            }
        }

    private:
        RenderDevice& m_renderDevice;
        // 各キューの数
        static const uint32_t k_graphicsQueueCount = 1;///> 
        static const uint32_t k_computeQueueCount = 4; ///>
        static const uint32_t k_copyQueueCount = 2;    ///>
        std::mutex m_graphicsMutex;
        std::condition_variable m_graphicsCV;
        std::queue<std::unique_ptr<GraphicsQueueContext>> m_graphicsQueuePool;
        std::mutex m_computeMutex;
        std::condition_variable m_computeCV;
        std::queue<std::unique_ptr<ComputeQueueContext>> m_computeQueuePool;
        std::mutex m_copyMutex;
        std::condition_variable m_copyCV;
        std::queue<std::unique_ptr<CopyQueueContext>> m_copyQueuePool;

        // Present用
        std::unique_ptr<GraphicsQueueContext> m_presentQueue;
    };
} // namespace Drama::Graphics::DX12

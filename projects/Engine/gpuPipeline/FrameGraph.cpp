#include "pch.h"
#include "FrameGraph.h"

#include <algorithm>
#include <queue>
#include <cwchar>

#include "Core/IO/public/LogAssert.h"
#include "GraphicsCore/public/RenderDevice.h"
#include "GraphicsCore/public/stdafx.h"

namespace Drama::Graphics
{
    namespace
    {
#ifndef NDEBUG
        class GpuEventScope final
        {
        public:
            GpuEventScope(ID3D12GraphicsCommandList* commandList, const wchar_t* name)
                : m_commandList(commandList)
            {
                // 1) PIX イベントを開始する
                if (m_commandList != nullptr)
                {
                    m_commandList->BeginEvent(0, name, static_cast<UINT>(wcslen(name) * sizeof(wchar_t)));
                }
            }
            ~GpuEventScope()
            {
                // 1) PIX イベントを終了する
                if (m_commandList != nullptr)
                {
                    m_commandList->EndEvent();
                }
            }
        private:
            ID3D12GraphicsCommandList* m_commandList = nullptr;
        };

#endif

    }

    struct FrameGraph::ResourceEntry final
    {
        ResourceKind m_kind = ResourceKind::Texture;
        ResourceLifetime m_lifetime = ResourceLifetime::Transient;
        uint16_t m_generation = 0;
        std::string m_name;

        DX12::ComPtr<ID3D12Resource> m_ownedResource;
        ID3D12Resource* m_externalResource = nullptr;

        D3D12_RESOURCE_STATES m_initialState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_STATES m_currentState = D3D12_RESOURCE_STATE_COMMON;

        DX12::DescriptorAllocator::TableID m_rtv{};
        DX12::DescriptorAllocator::TableID m_dsv{};
        DX12::DescriptorAllocator::TableID m_srv{};
        DX12::DescriptorAllocator::TableID m_uav{};
        bool m_rtvOwned = false;
        bool m_dsvOwned = false;
        bool m_srvOwned = false;
        bool m_uavOwned = false;

        bool m_allowRenderTarget = false;
        bool m_allowDepthStencil = false;
        bool m_allowUnorderedAccess = false;

        ID3D12Resource* get_resource() const
        {
            // 1) 外部参照がある場合はそれを返す
            // 2) なければ所有リソースを返す
            if (m_externalResource != nullptr)
            {
                return m_externalResource;
            }
            return m_ownedResource.Get();
        }
    };

    struct FrameGraph::ResourceAccess final
    {
        ResourceHandle m_handle{};
        D3D12_RESOURCE_STATES m_state = D3D12_RESOURCE_STATE_COMMON;
        ResourceAccessType m_access = ResourceAccessType::Read;
        bool m_hasFinalState = false;
        D3D12_RESOURCE_STATES m_finalState = D3D12_RESOURCE_STATE_COMMON;
    };

    struct FrameGraph::PassNode final
    {
        FrameGraphPass* m_pass = nullptr;
        PassType m_type = PassType::Render;
        bool m_allowAsyncCompute = false;
        bool m_allowCopyQueue = false;
        std::vector<ResourceAccess> m_accesses;
        std::vector<uint32_t> m_dependencies;
    };

#ifndef NDEBUG
    class FrameGraph::FrameGraphProfiler final
    {
    public:
        FrameGraphProfiler(DX12::RenderDevice& renderDevice)
            : m_renderDevice(renderDevice)
        {
            // 1) RenderDevice を保持して後続の計測に使う
        }

        void set_frames_in_flight(uint32_t framesInFlight)
        {
            // 1) 最低 1 を保証して保存する
            // 2) フェンス配列を再初期化する
            m_framesInFlight = (framesInFlight == 0) ? 1 : framesInFlight;
            m_frameFenceValues.assign(m_framesInFlight, 0);
            m_passCounts.assign(m_framesInFlight, 0);
        }

        void begin_frame(uint32_t frameIndex, uint32_t passCount)
        {
            // 1) フレームインデックスを更新する
            // 2) 必要なら計測用バッファを確保する
            m_frameIndex = frameIndex % m_framesInFlight;
            ensure_capacity(passCount);
            if (m_frameIndex < m_passCounts.size())
            {
                m_passCounts[m_frameIndex] = passCount;
            }

            if (m_timestampFrequency == 0)
            {
                auto* queuePool = m_renderDevice.get_queue_pool();
                auto* graphicsQueue = queuePool->get_graphics_queue();
                if (graphicsQueue)
                {
                    graphicsQueue->get_command_queue()->GetTimestampFrequency(&m_timestampFrequency);
                    queuePool->return_queue(graphicsQueue);
                }
            }
        }

        void begin_pass(uint32_t passIndex, ID3D12GraphicsCommandList* commandList)
        {
            // 1) 開始タイムスタンプを記録する
            if (!commandList || !m_queryHeap)
            {
                return;
            }
            const uint32_t queryIndex = get_query_index(passIndex, 0);
            commandList->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, queryIndex);
        }

        void end_pass(uint32_t passIndex, ID3D12GraphicsCommandList* commandList)
        {
            // 1) 終了タイムスタンプを記録する
            if (!commandList || !m_queryHeap)
            {
                return;
            }
            const uint32_t queryIndex = get_query_index(passIndex, 1);
            commandList->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, queryIndex);
        }

        void resolve_pass(uint32_t passIndex, ID3D12GraphicsCommandList* commandList)
        {
            // 1) 記録済みのクエリを Readback に解決する
            if (!commandList || !m_queryHeap || !m_readback)
            {
                return;
            }
            const uint32_t startIndex = get_query_index(passIndex, 0);
            const uint64_t offsetBytes = static_cast<uint64_t>(startIndex) * sizeof(uint64_t);
            commandList->ResolveQueryData(
                m_queryHeap.Get(),
                D3D12_QUERY_TYPE_TIMESTAMP,
                startIndex,
                2,
                m_readback.Get(),
                offsetBytes);
        }

        void mark_frame_fence(ID3D12Fence* fence, uint64_t fenceValue)
        {
            // 1) フェンスを保持して遅延読み出しに使う
            // 2) 現フレームの完了値を記録する
            m_fence = fence;
            if (m_frameIndex < m_frameFenceValues.size())
            {
                m_frameFenceValues[m_frameIndex] = fenceValue;
            }
        }

        void try_collect()
        {
            // 1) 1 フレーム遅延分が完了しているか確認する
            // 2) 完了していれば Readback を読み取る
            if (!m_fence || !m_readback || m_framesInFlight == 0)
            {
                return;
            }
            const uint32_t readIndex = (m_frameIndex + m_framesInFlight - 1) % m_framesInFlight;
            if (readIndex >= m_frameFenceValues.size() || readIndex >= m_passCounts.size())
            {
                return;
            }
            const uint64_t fenceValue = m_frameFenceValues[readIndex];
            if (fenceValue == 0 || m_fence->GetCompletedValue() < fenceValue)
            {
                return;
            }
            const uint32_t passCount = m_passCounts[readIndex];
            if (passCount == 0)
            {
                return;
            }

            uint64_t* data = nullptr;
            const uint64_t offset = static_cast<uint64_t>(readIndex) * m_queriesPerFrame * sizeof(uint64_t);
            D3D12_RANGE range{};
            range.Begin = offset;
            range.End = offset + static_cast<uint64_t>(passCount * 2) * sizeof(uint64_t);
            if (SUCCEEDED(m_readback->Map(0, &range, reinterpret_cast<void**>(&data))))
            {
                // 1) ここで結果を利用する場合は pass 単位で差分計算する
                // 2) 収集後にアンマップして安全を保つ
                m_readback->Unmap(0, nullptr);
            }
        }

    private:
        void ensure_capacity(uint32_t passCount)
        {
            // 1) 必要なクエリ数が足りているか確認する
            // 2) 足りなければ QueryHeap と Readback を作り直す
            if (passCount == 0)
            {
                return;
            }
            if (passCount <= m_passCapacity && m_queryHeap && m_readback)
            {
                return;
            }

            m_passCapacity = passCount;
            m_queriesPerFrame = m_passCapacity * 2;
            const uint32_t totalQueries = m_queriesPerFrame * m_framesInFlight;

            D3D12_QUERY_HEAP_DESC heapDesc{};
            heapDesc.Count = totalQueries;
            heapDesc.NodeMask = 0;
            heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;

            m_queryHeap.Reset();
            HRESULT hr = m_renderDevice.get_d3d12_device()->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&m_queryHeap));
            if (FAILED(hr))
            {
                m_queryHeap.Reset();
                return;
            }

            D3D12_RESOURCE_DESC bufferDesc{};
            bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            bufferDesc.Width = static_cast<UINT64>(totalQueries) * sizeof(uint64_t);
            bufferDesc.Height = 1;
            bufferDesc.DepthOrArraySize = 1;
            bufferDesc.MipLevels = 1;
            bufferDesc.SampleDesc.Count = 1;
            bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

            D3D12_HEAP_PROPERTIES heapProps{};
            heapProps.Type = D3D12_HEAP_TYPE_READBACK;

            m_readback.Reset();
            hr = m_renderDevice.get_d3d12_device()->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &bufferDesc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&m_readback));
            if (FAILED(hr))
            {
                m_readback.Reset();
                return;
            }
        }

        uint32_t get_query_index(uint32_t passIndex, uint32_t offset) const
        {
            // 1) フレーム内のクエリ位置を算出する
            return (m_frameIndex * m_queriesPerFrame) + passIndex * 2 + offset;
        }

    private:
        DX12::RenderDevice& m_renderDevice;
        DX12::ComPtr<ID3D12QueryHeap> m_queryHeap;
        DX12::ComPtr<ID3D12Resource> m_readback;
        ID3D12Fence* m_fence = nullptr;
        uint64_t m_timestampFrequency = 0;
        uint32_t m_framesInFlight = 1;
        uint32_t m_passCapacity = 0;
        uint32_t m_queriesPerFrame = 0;
        uint32_t m_frameIndex = 0;
        std::vector<uint64_t> m_frameFenceValues;
        std::vector<uint32_t> m_passCounts;
    };
#endif


    FrameGraph::FrameGraph(
        DX12::RenderDevice& renderDevice,
        DX12::DescriptorAllocator& descriptorAllocator,
        DX12::CommandPool& commandPool)
        : m_renderDevice(renderDevice)
        , m_descriptorAllocator(descriptorAllocator)
        , m_commandPool(commandPool)
    {
        // 1) 依存リソースを保持して後続処理に備える
#ifndef NDEBUG
        m_profiler = std::make_unique<FrameGraphProfiler>(renderDevice);
        m_profiler->set_frames_in_flight(m_framesInFlight);
#endif
    }

    FrameGraph::~FrameGraph() = default;

    void FrameGraph::set_frames_in_flight(uint32_t framesInFlight)
    {
        // 1) 最低 1 を保証して設定する
        m_framesInFlight = (framesInFlight == 0) ? 1 : framesInFlight;
#ifndef NDEBUG
        if (m_profiler)
        {
            m_profiler->set_frames_in_flight(m_framesInFlight);
        }
#endif
    }

    void FrameGraph::set_async_compute_enabled(bool enabled)
    {
        // 1) フラグを保持してキュー選択に使う
        m_asyncComputeEnabled = enabled;
    }

    void FrameGraph::set_copy_queue_enabled(bool enabled)
    {
        // 1) フラグを保持してキュー選択に使う
        m_copyQueueEnabled = enabled;
    }

    void FrameGraph::reset(uint64_t frameNo, uint32_t frameIndex)
    {
        // 1) 前フレームの一時リソースを解放する
        // 2) 登録情報を破棄して次フレームへ切り替える
        release_transient_descriptors();
        m_passes.clear();
        m_resources.clear();
        m_executionOrder.clear();
        m_pipelineRequests.clear();
        m_frameNo = frameNo;
        m_frameIndex = frameIndex % m_framesInFlight;
    }

    void FrameGraph::add_pass(FrameGraphPass& pass)
    {
        // 1) パス情報を記録して後続の依存解決に使う
        PassNode node{};
        node.m_pass = &pass;
        node.m_type = pass.get_pass_type();
        node.m_allowAsyncCompute = pass.allow_async_compute();
        node.m_allowCopyQueue = pass.allow_copy_queue();
        m_passes.push_back(std::move(node));
    }

    Core::Error::Result FrameGraph::build()
    {
        // 1) パスの setup を順に実行して宣言を集める
        // 2) 依存解決と実行順を確定する
        for (uint32_t i = 0; i < m_passes.size(); ++i)
        {
            FrameGraphBuilder builder(*this, i);
            m_passes[i].m_pass->setup(builder);
            m_passes[i].m_pass->pipeline_requests(m_pipelineRequests);
        }

        Core::Error::Result result = build_dependencies();
        if (!result)
        {
            return result;
        }
        return build_execution_order();
    }

    Core::Error::Result FrameGraph::execute(FrameGraphExecutionInfo& outInfo)
    {
        // 1) リソース状態を初期化する
        // 2) 実行順にパスを記録して送信する
        reset_resource_states();
        outInfo = FrameGraphExecutionInfo{};
        std::vector<PassExecutionInfo> passExecution(m_passes.size());

#ifndef NDEBUG
        if (m_profiler)
        {
            m_profiler->begin_frame(m_frameIndex, static_cast<uint32_t>(m_passes.size()));
        }
#endif

        for (uint32_t orderIndex = 0; orderIndex < m_executionOrder.size(); ++orderIndex)
        {
            const uint32_t passIndex = m_executionOrder[orderIndex];
            Core::Error::Result result = execute_pass(passIndex, outInfo, passExecution);
            if (!result)
            {
                return result;
            }
        }

#ifndef NDEBUG
        if (m_profiler && outInfo.m_graphicsFence != nullptr)
        {
            m_profiler->mark_frame_fence(outInfo.m_graphicsFence, outInfo.m_graphicsFenceValue);
            m_profiler->try_collect();
        }
#endif

        return Core::Error::Result::ok();
    }

    ResourceHandle FrameGraph::create_transient_texture(const TransientTextureDesc& desc, const char* name)
    {
        // 1) リソース情報を構築して登録する
        // 2) Transient リソースとして生成する
        ResourceEntry entry{};
        entry.m_kind = ResourceKind::Texture;
        entry.m_lifetime = ResourceLifetime::Transient;
        entry.m_name = (name != nullptr) ? name : "TransientTexture";
        entry.m_initialState = D3D12_RESOURCE_STATE_COMMON;
        entry.m_currentState = D3D12_RESOURCE_STATE_COMMON;
        entry.m_allowRenderTarget = desc.m_allowRenderTarget;
        entry.m_allowDepthStencil = desc.m_allowDepthStencil;
        entry.m_allowUnorderedAccess = desc.m_allowUnorderedAccess;

        D3D12_RESOURCE_DESC resourceDesc = desc.m_desc;
        if (entry.m_allowRenderTarget)
        {
            resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        }
        if (entry.m_allowDepthStencil)
        {
            resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        }
        if (entry.m_allowUnorderedAccess)
        {
            resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_CLEAR_VALUE* clearValue = desc.m_useClearValue ? const_cast<D3D12_CLEAR_VALUE*>(&desc.m_clearValue) : nullptr;

        DX12::ComPtr<ID3D12Resource> resource;
        HRESULT hr = m_renderDevice.get_d3d12_device()->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_COMMON,
            clearValue,
            IID_PPV_ARGS(&resource));
        if (FAILED(hr))
        {
            return ResourceHandle{};
        }

        if (name != nullptr)
        {
            const std::wstring wideName = DX12::to_utf16(name);
            DX12::SetD3D12Name(resource.Get(), wideName.c_str());
        }

        entry.m_ownedResource = std::move(resource);

        if (entry.m_allowRenderTarget)
        {
            entry.m_rtv = m_descriptorAllocator.allocate(DX12::DescriptorAllocator::TableKind::RenderTargets);
            entry.m_rtvOwned = true;
            m_descriptorAllocator.create_rtv(entry.m_rtv, entry.get_resource());
        }
        if (entry.m_allowDepthStencil)
        {
            entry.m_dsv = m_descriptorAllocator.allocate(DX12::DescriptorAllocator::TableKind::DepthStencils);
            entry.m_dsvOwned = true;
            m_descriptorAllocator.create_dsv(entry.m_dsv, entry.get_resource());
        }

        ResourceHandle handle{};
        handle.m_index = static_cast<uint32_t>(m_resources.size());
        handle.m_generation = entry.m_generation;
        m_resources.push_back(std::move(entry));
        return handle;
    }

    ResourceHandle FrameGraph::create_transient_buffer(const TransientBufferDesc& desc, const char* name)
    {
        // 1) バッファ用のリソースを生成する
        // 2) Transient として登録する
        ResourceEntry entry{};
        entry.m_kind = ResourceKind::Buffer;
        entry.m_lifetime = ResourceLifetime::Transient;
        entry.m_name = (name != nullptr) ? name : "TransientBuffer";
        entry.m_initialState = D3D12_RESOURCE_STATE_COMMON;
        entry.m_currentState = D3D12_RESOURCE_STATE_COMMON;

        D3D12_RESOURCE_DESC bufferDesc{};
        bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Width = desc.m_size;
        bufferDesc.Height = 1;
        bufferDesc.DepthOrArraySize = 1;
        bufferDesc.MipLevels = 1;
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufferDesc.Flags = desc.m_flags;

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        DX12::ComPtr<ID3D12Resource> resource;
        HRESULT hr = m_renderDevice.get_d3d12_device()->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&resource));
        if (FAILED(hr))
        {
            return ResourceHandle{};
        }

        if (name != nullptr)
        {
            const std::wstring wideName = DX12::to_utf16(name);
            DX12::SetD3D12Name(resource.Get(), wideName.c_str());
        }

        entry.m_ownedResource = std::move(resource);

        ResourceHandle handle{};
        handle.m_index = static_cast<uint32_t>(m_resources.size());
        handle.m_generation = entry.m_generation;
        m_resources.push_back(std::move(entry));
        return handle;
    }

    ResourceHandle FrameGraph::import_texture(ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState,
        const DX12::DescriptorAllocator::TableID& rtvTable, const char* name)
    {
        // 1) 外部リソースとして登録する
        ResourceEntry entry{};
        entry.m_kind = ResourceKind::Texture;
        entry.m_lifetime = ResourceLifetime::Imported;
        entry.m_name = (name != nullptr) ? name : "ImportedTexture";
        entry.m_externalResource = resource;
        entry.m_initialState = initialState;
        entry.m_currentState = initialState;
        entry.m_rtv = rtvTable;

        ResourceHandle handle{};
        handle.m_index = static_cast<uint32_t>(m_resources.size());
        handle.m_generation = entry.m_generation;
        m_resources.push_back(std::move(entry));
        return handle;
    }

    ResourceHandle FrameGraph::import_buffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState, const char* name)
    {
        // 1) 外部バッファとして登録する
        ResourceEntry entry{};
        entry.m_kind = ResourceKind::Buffer;
        entry.m_lifetime = ResourceLifetime::Imported;
        entry.m_name = (name != nullptr) ? name : "ImportedBuffer";
        entry.m_externalResource = resource;
        entry.m_initialState = initialState;
        entry.m_currentState = initialState;

        ResourceHandle handle{};
        handle.m_index = static_cast<uint32_t>(m_resources.size());
        handle.m_generation = entry.m_generation;
        m_resources.push_back(std::move(entry));
        return handle;
    }

    void FrameGraph::read_texture(ResourceHandle handle, D3D12_RESOURCE_STATES state, uint32_t passIndex)
    {
        // 1) ハンドル妥当性を検証する
        // 2) アクセス情報を登録する
        Core::Error::Result result = validate_handle(handle, ResourceKind::Texture);
        Core::IO::LogAssert::assert(result, "Invalid texture handle.");
        ResourceAccess access{};
        access.m_handle = handle;
        access.m_state = state;
        access.m_access = ResourceAccessType::Read;
        m_passes[passIndex].m_accesses.push_back(access);
    }

    void FrameGraph::write_texture(ResourceHandle handle, D3D12_RESOURCE_STATES state, uint32_t passIndex)
    {
        // 1) ハンドル妥当性を検証する
        // 2) アクセス情報を登録する
        write_texture(handle, state, state, passIndex);
    }

    void FrameGraph::write_texture(ResourceHandle handle, D3D12_RESOURCE_STATES state, D3D12_RESOURCE_STATES finalState, uint32_t passIndex)
    {
        // 1) ハンドル妥当性を検証する
        // 2) アクセス情報を登録する
        Core::Error::Result result = validate_handle(handle, ResourceKind::Texture);
        Core::IO::LogAssert::assert(result, "Invalid texture handle.");
        ResourceAccess access{};
        access.m_handle = handle;
        access.m_state = state;
        access.m_access = ResourceAccessType::Write;
        access.m_hasFinalState = (finalState != state);
        access.m_finalState = finalState;
        m_passes[passIndex].m_accesses.push_back(access);
    }

    void FrameGraph::read_buffer(ResourceHandle handle, D3D12_RESOURCE_STATES state, uint32_t passIndex)
    {
        // 1) ハンドル妥当性を検証する
        // 2) アクセス情報を登録する
        Core::Error::Result result = validate_handle(handle, ResourceKind::Buffer);
        Core::IO::LogAssert::assert(result, "Invalid buffer handle.");
        ResourceAccess access{};
        access.m_handle = handle;
        access.m_state = state;
        access.m_access = ResourceAccessType::Read;
        m_passes[passIndex].m_accesses.push_back(access);
    }

    void FrameGraph::write_buffer(ResourceHandle handle, D3D12_RESOURCE_STATES state, uint32_t passIndex)
    {
        // 1) ハンドル妥当性を検証する
        // 2) アクセス情報を登録する
        write_buffer(handle, state, state, passIndex);
    }

    void FrameGraph::write_buffer(ResourceHandle handle, D3D12_RESOURCE_STATES state, D3D12_RESOURCE_STATES finalState, uint32_t passIndex)
    {
        // 1) ハンドル妥当性を検証する
        // 2) アクセス情報を登録する
        Core::Error::Result result = validate_handle(handle, ResourceKind::Buffer);
        Core::IO::LogAssert::assert(result, "Invalid buffer handle.");
        ResourceAccess access{};
        access.m_handle = handle;
        access.m_state = state;
        access.m_access = ResourceAccessType::Write;
        access.m_hasFinalState = (finalState != state);
        access.m_finalState = finalState;
        m_passes[passIndex].m_accesses.push_back(access);
    }

    ID3D12Resource* FrameGraph::get_resource(ResourceHandle handle)
    {
        // 1) ハンドル妥当性を確認する
        // 2) 対象リソースを返す
        if (!handle.valid() || handle.m_index >= m_resources.size())
        {
            Core::IO::LogAssert::assert(false, "Invalid resource handle.");
            return nullptr;
        }
        ResourceEntry& entry = m_resources[handle.m_index];
        if (entry.m_generation != handle.m_generation)
        {
            Core::IO::LogAssert::assert(false, "Invalid resource generation.");
            return nullptr;
        }
        return entry.get_resource();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE FrameGraph::get_rtv(ResourceHandle handle) const
    {
        // 1) RTV が確保済みであることを前提にハンドルを返す
        if (!handle.valid() || handle.m_index >= m_resources.size())
        {
            return D3D12_CPU_DESCRIPTOR_HANDLE{};
        }
        const ResourceEntry& entry = m_resources[handle.m_index];
        return m_descriptorAllocator.get_cpu_handle(entry.m_rtv);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE FrameGraph::get_dsv(ResourceHandle handle) const
    {
        // 1) DSV が確保済みであることを前提にハンドルを返す
        if (!handle.valid() || handle.m_index >= m_resources.size())
        {
            return D3D12_CPU_DESCRIPTOR_HANDLE{};
        }
        const ResourceEntry& entry = m_resources[handle.m_index];
        return m_descriptorAllocator.get_cpu_handle(entry.m_dsv);
    }

    DX12::QueueType FrameGraph::select_queue(const PassNode& pass) const
    {
        // 1) PassType と許可設定から実行キューを決定する
        if (pass.m_type == PassType::Render)
        {
            return DX12::QueueType::Graphics;
        }
        if (pass.m_type == PassType::Compute)
        {
            if (m_asyncComputeEnabled && pass.m_allowAsyncCompute)
            {
                return DX12::QueueType::Compute;
            }
            return DX12::QueueType::Graphics;
        }
        if (pass.m_type == PassType::Copy)
        {
            if (m_copyQueueEnabled && pass.m_allowCopyQueue)
            {
                return DX12::QueueType::Copy;
            }
            return DX12::QueueType::Graphics;
        }
        return DX12::QueueType::Graphics;
    }

    void FrameGraph::release_transient_descriptors()
    {
        // 1) Transient のテーブルを回収する
        for (auto& resource : m_resources)
        {
            if (resource.m_lifetime != ResourceLifetime::Transient)
            {
                continue;
            }
            if (resource.m_rtvOwned && resource.m_rtv.valid())
            {
                m_descriptorAllocator.free_table(resource.m_rtv);
            }
            if (resource.m_dsvOwned && resource.m_dsv.valid())
            {
                m_descriptorAllocator.free_table(resource.m_dsv);
            }
            if (resource.m_srvOwned && resource.m_srv.valid())
            {
                m_descriptorAllocator.free_table(resource.m_srv);
            }
            if (resource.m_uavOwned && resource.m_uav.valid())
            {
                m_descriptorAllocator.free_table(resource.m_uav);
            }
        }
    }

    Core::Error::Result FrameGraph::build_dependencies()
    {
        // 1) 各リソースの直近アクセスを追跡する
        // 2) Read/Write の順序から依存を作成する
        const uint32_t passCount = static_cast<uint32_t>(m_passes.size());
        const uint32_t resourceCount = static_cast<uint32_t>(m_resources.size());
        std::vector<int32_t> lastWriter(resourceCount, -1);
        std::vector<std::vector<uint32_t>> lastReaders(resourceCount);

        for (uint32_t passIndex = 0; passIndex < passCount; ++passIndex)
        {
            PassNode& pass = m_passes[passIndex];
            for (const auto& access : pass.m_accesses)
            {
                const uint32_t resourceIndex = access.m_handle.m_index;
                if (resourceIndex >= resourceCount)
                {
                    return Core::Error::Result::fail(
                        Core::Error::Facility::Graphics,
                        Core::Error::Code::InvalidArg,
                        Core::Error::Severity::Error,
                        0,
                        "FrameGraph resource index out of range.");
                }

                if (access.m_access == ResourceAccessType::Read)
                {
                    if (lastWriter[resourceIndex] < 0 &&
                        m_resources[resourceIndex].m_lifetime == ResourceLifetime::Transient)
                    {
                        return Core::Error::Result::fail(
                            Core::Error::Facility::Graphics,
                            Core::Error::Code::InvalidState,
                            Core::Error::Severity::Error,
                            0,
                            "Transient resource read before write.");
                    }
                    if (lastWriter[resourceIndex] >= 0)
                    {
                        pass.m_dependencies.push_back(static_cast<uint32_t>(lastWriter[resourceIndex]));
                    }
                    lastReaders[resourceIndex].push_back(passIndex);
                }
                else
                {
                    if (lastWriter[resourceIndex] >= 0)
                    {
                        pass.m_dependencies.push_back(static_cast<uint32_t>(lastWriter[resourceIndex]));
                    }
                    for (uint32_t readerIndex : lastReaders[resourceIndex])
                    {
                        pass.m_dependencies.push_back(readerIndex);
                    }
                    lastReaders[resourceIndex].clear();
                    lastWriter[resourceIndex] = static_cast<int32_t>(passIndex);
                }
            }
        }

        return Core::Error::Result::ok();
    }

    Core::Error::Result FrameGraph::build_execution_order()
    {
        // 1) 依存からトポロジカル順序を構築する
        // 2) サイクルがあればエラーで返す
        const uint32_t passCount = static_cast<uint32_t>(m_passes.size());
        std::vector<uint32_t> indegrees(passCount, 0);
        std::vector<std::vector<uint32_t>> outgoing(passCount);

        for (uint32_t i = 0; i < passCount; ++i)
        {
            auto& pass = m_passes[i];
            for (uint32_t dep : pass.m_dependencies)
            {
                if (dep >= passCount)
                {
                    continue;
                }
                outgoing[dep].push_back(i);
                indegrees[i]++;
            }
        }

        std::queue<uint32_t> ready;
        for (uint32_t i = 0; i < passCount; ++i)
        {
            if (indegrees[i] == 0)
            {
                ready.push(i);
            }
        }

        m_executionOrder.clear();
        while (!ready.empty())
        {
            uint32_t node = ready.front();
            ready.pop();
            m_executionOrder.push_back(node);
            for (uint32_t out : outgoing[node])
            {
                if (--indegrees[out] == 0)
                {
                    ready.push(out);
                }
            }
        }

        if (m_executionOrder.size() != passCount)
        {
            return Core::Error::Result::fail(
                Core::Error::Facility::Graphics,
                Core::Error::Code::InvalidState,
                Core::Error::Severity::Error,
                0,
                "FrameGraph dependency cycle detected.");
        }

        return Core::Error::Result::ok();
    }

    void FrameGraph::reset_resource_states()
    {
        // 1) 初期状態へ戻して次の実行準備を整える
        for (auto& resource : m_resources)
        {
            resource.m_currentState = resource.m_initialState;
        }
    }

    Core::Error::Result FrameGraph::execute_pass(uint32_t passIndex, FrameGraphExecutionInfo& outInfo,
        std::vector<PassExecutionInfo>& passExecution)
    {
        // 1) キューとコマンドコンテキストを準備する
        // 2) バリアとパス実行を記録して送信する
        if (passIndex >= m_passes.size())
        {
            return Core::Error::Result::fail(
                Core::Error::Facility::Graphics,
                Core::Error::Code::InvalidArg,
                Core::Error::Severity::Error,
                0,
                "FrameGraph pass index out of range.");
        }

        PassNode& pass = m_passes[passIndex];
        DX12::QueueType queueType = select_queue(pass);

        DX12::QueuePool* queuePool = m_renderDevice.get_queue_pool();
        DX12::QueueContext* queueContext = nullptr;
        if (queueType == DX12::QueueType::Graphics)
        {
            queueContext = queuePool->get_graphics_queue();
        }
        else if (queueType == DX12::QueueType::Compute)
        {
            queueContext = queuePool->get_compute_queue();
        }
        else
        {
            queueContext = queuePool->get_copy_queue();
        }

        if (!queueContext)
        {
            return Core::Error::Result::fail(
                Core::Error::Facility::Graphics,
                Core::Error::Code::InvalidState,
                Core::Error::Severity::Error,
                0,
                "QueueContext not available.");
        }

        DX12::CommandContext* commandContext = nullptr;
        if (queueType == DX12::QueueType::Graphics)
        {
            commandContext = m_commandPool.get_graphics_context();
        }
        else if (queueType == DX12::QueueType::Compute)
        {
            commandContext = m_commandPool.get_compute_context();
        }
        else
        {
            commandContext = m_commandPool.get_copy_context();
        }

        if (!commandContext)
        {
            return Core::Error::Result::fail(
                Core::Error::Facility::Graphics,
                Core::Error::Code::InvalidState,
                Core::Error::Severity::Error,
                0,
                "CommandContext not available.");
        }

        // 依存パスのキューと同期する
        for (uint32_t dep : pass.m_dependencies)
        {
            if (dep >= m_passes.size())
            {
                continue;
            }
            const PassExecutionInfo& depInfo = passExecution[dep];
            if (depInfo.m_queue == nullptr)
            {
                continue;
            }
            if (depInfo.m_queue == queueContext)
            {
                continue;
            }
            queueContext->get_command_queue()->Wait(depInfo.m_queue->get_fence(), depInfo.m_fenceValue);
        }

        commandContext->reset();
        ID3D12GraphicsCommandList* commandList = commandContext->get_command_list();

        std::vector<D3D12_RESOURCE_BARRIER> barriers;
        collect_barriers(pass, barriers);
        if (!barriers.empty())
        {
            commandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
        }

#ifndef NDEBUG
        const std::wstring passName = DX12::to_utf16(pass.m_pass->get_name());
        GpuEventScope eventScope(commandList, passName.c_str());
        if (m_profiler)
        {
            m_profiler->begin_pass(passIndex, commandList);
        }
#endif

        FrameGraphContext context(*this, commandList, queueType);
        pass.m_pass->execute(context);

#ifndef NDEBUG
        if (m_profiler)
        {
            m_profiler->end_pass(passIndex, commandList);
            m_profiler->resolve_pass(passIndex, commandList);
        }
#endif

        std::vector<D3D12_RESOURCE_BARRIER> postBarriers;
        for (const auto& access : pass.m_accesses)
        {
            if (!access.m_hasFinalState)
            {
                continue;
            }
            const uint32_t resourceIndex = access.m_handle.m_index;
            if (resourceIndex >= m_resources.size())
            {
                continue;
            }
            ResourceEntry& resource = m_resources[resourceIndex];
            ID3D12Resource* d3dResource = resource.get_resource();
            if (!d3dResource)
            {
                continue;
            }
            if (resource.m_currentState != access.m_finalState)
            {
                D3D12_RESOURCE_BARRIER barrier{};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                barrier.Transition.pResource = d3dResource;
                barrier.Transition.StateBefore = resource.m_currentState;
                barrier.Transition.StateAfter = access.m_finalState;
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                postBarriers.push_back(barrier);
                resource.m_currentState = access.m_finalState;
            }
        }

        if (!postBarriers.empty())
        {
            commandList->ResourceBarrier(static_cast<UINT>(postBarriers.size()), postBarriers.data());
        }

        commandContext->close();
        queueContext->execute(commandContext);

        if (queueType == DX12::QueueType::Graphics)
        {
            outInfo.m_graphicsFence = queueContext->get_fence();
            outInfo.m_graphicsFenceValue = queueContext->get_fence_value();
        }

        passExecution[passIndex].m_queue = queueContext;
        passExecution[passIndex].m_fenceValue = queueContext->get_fence_value();

        if (queueType == DX12::QueueType::Graphics)
        {
            queuePool->return_queue(static_cast<DX12::GraphicsQueueContext*>(queueContext));
            m_commandPool.return_context(static_cast<DX12::GraphicsCommandContext*>(commandContext));
        }
        else if (queueType == DX12::QueueType::Compute)
        {
            queuePool->return_queue(static_cast<DX12::ComputeQueueContext*>(queueContext));
            m_commandPool.return_context(static_cast<DX12::ComputeCommandContext*>(commandContext));
        }
        else
        {
            queuePool->return_queue(static_cast<DX12::CopyQueueContext*>(queueContext));
            m_commandPool.return_context(static_cast<DX12::CopyCommandContext*>(commandContext));
        }

        return Core::Error::Result::ok();
    }

    void FrameGraph::collect_barriers(const PassNode& pass, std::vector<D3D12_RESOURCE_BARRIER>& outBarriers)
    {
        // 1) パスのアクセスに応じて必要なバリアを構築する
        std::vector<uint32_t> processed;
        processed.reserve(pass.m_accesses.size());

        for (const auto& access : pass.m_accesses)
        {
            const uint32_t resourceIndex = access.m_handle.m_index;
            if (resourceIndex >= m_resources.size())
            {
                continue;
            }
            if (std::find(processed.begin(), processed.end(), resourceIndex) != processed.end())
            {
                continue;
            }

            ResourceEntry& resource = m_resources[resourceIndex];
            ID3D12Resource* d3dResource = resource.get_resource();
            if (!d3dResource)
            {
                continue;
            }

            if (resource.m_currentState != access.m_state)
            {
                D3D12_RESOURCE_BARRIER barrier{};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                barrier.Transition.pResource = d3dResource;
                barrier.Transition.StateBefore = resource.m_currentState;
                barrier.Transition.StateAfter = access.m_state;
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                outBarriers.push_back(barrier);
                resource.m_currentState = access.m_state;
            }

            if (access.m_access == ResourceAccessType::Write &&
                (access.m_state & D3D12_RESOURCE_STATE_UNORDERED_ACCESS) != 0)
            {
                D3D12_RESOURCE_BARRIER uavBarrier{};
                uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                uavBarrier.UAV.pResource = d3dResource;
                outBarriers.push_back(uavBarrier);
            }

            processed.push_back(resourceIndex);
        }
    }

    Core::Error::Result FrameGraph::validate_handle(ResourceHandle handle, ResourceKind kind) const
    {
        // 1) ハンドル範囲と世代を検証する
        // 2) 種別が一致するか確認する
        if (!handle.valid() || handle.m_index >= m_resources.size())
        {
            return Core::Error::Result::fail(
                Core::Error::Facility::Graphics,
                Core::Error::Code::InvalidArg,
                Core::Error::Severity::Error,
                0,
                "FrameGraph resource handle invalid.");
        }

        const ResourceEntry& entry = m_resources[handle.m_index];
        if (entry.m_generation != handle.m_generation)
        {
            return Core::Error::Result::fail(
                Core::Error::Facility::Graphics,
                Core::Error::Code::InvalidState,
                Core::Error::Severity::Error,
                0,
                "FrameGraph resource generation mismatch.");
        }

        if (entry.m_kind != kind)
        {
            return Core::Error::Result::fail(
                Core::Error::Facility::Graphics,
                Core::Error::Code::InvalidArg,
                Core::Error::Severity::Error,
                0,
                "FrameGraph resource kind mismatch.");
        }

        return Core::Error::Result::ok();
    }

    ResourceHandle FrameGraphBuilder::create_transient_texture(const TransientTextureDesc& desc, const char* name)
    {
        // 1) FrameGraph に登録してハンドルを返す
        return m_graph.create_transient_texture(desc, name);
    }

    ResourceHandle FrameGraphBuilder::create_transient_buffer(const TransientBufferDesc& desc, const char* name)
    {
        // 1) FrameGraph に登録してハンドルを返す
        return m_graph.create_transient_buffer(desc, name);
    }

    ResourceHandle FrameGraphBuilder::import_texture(ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState,
        const DX12::DescriptorAllocator::TableID& rtvTable, const char* name)
    {
        // 1) 外部リソースを FrameGraph に登録する
        return m_graph.import_texture(resource, initialState, rtvTable, name);
    }

    ResourceHandle FrameGraphBuilder::import_buffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState, const char* name)
    {
        // 1) 外部リソースを FrameGraph に登録する
        return m_graph.import_buffer(resource, initialState, name);
    }

    void FrameGraphBuilder::read_texture(ResourceHandle handle, D3D12_RESOURCE_STATES state)
    {
        // 1) 読み取りアクセスを登録する
        m_graph.read_texture(handle, state, m_passIndex);
    }

    void FrameGraphBuilder::write_texture(ResourceHandle handle, D3D12_RESOURCE_STATES state)
    {
        // 1) 書き込みアクセスを登録する
        m_graph.write_texture(handle, state, m_passIndex);
    }

    void FrameGraphBuilder::write_texture(ResourceHandle handle, D3D12_RESOURCE_STATES state, D3D12_RESOURCE_STATES finalState)
    {
        // 1) 書き込みアクセスと最終状態を登録する
        m_graph.write_texture(handle, state, finalState, m_passIndex);
    }

    void FrameGraphBuilder::read_buffer(ResourceHandle handle, D3D12_RESOURCE_STATES state)
    {
        // 1) 読み取りアクセスを登録する
        m_graph.read_buffer(handle, state, m_passIndex);
    }

    void FrameGraphBuilder::write_buffer(ResourceHandle handle, D3D12_RESOURCE_STATES state)
    {
        // 1) 書き込みアクセスを登録する
        m_graph.write_buffer(handle, state, m_passIndex);
    }

    void FrameGraphBuilder::write_buffer(ResourceHandle handle, D3D12_RESOURCE_STATES state, D3D12_RESOURCE_STATES finalState)
    {
        // 1) 書き込みアクセスと最終状態を登録する
        m_graph.write_buffer(handle, state, finalState, m_passIndex);
    }
}

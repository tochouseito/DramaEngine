#pragma once

// === C++ Standard Library ===
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

// === Engine ===
#include "Core/Error/Result.h"
#include "Engine/interface/ExportsMacro.h"
#include "GraphicsCore/public/GpuCommand.h"
#include "GraphicsCore/public/DescriptorAllocator.h"

namespace Drama::Graphics
{
    class FrameGraphBuilder;
    class FrameGraphContext;
    class PipelineRequestCollector;

    enum class PassType : uint8_t
    {
        Render,
        Compute,
        Copy
    };

    enum class ResourceLifetime : uint8_t
    {
        Transient,// 一時的
        Persistent,// 永続的
        Imported // 外部
    };

    enum class ResourceKind : uint8_t
    {
        Texture,
        Buffer
    };

    enum class ResourceAccessType : uint8_t
    {
        Read,
        Write
    };

    struct ResourceHandle final
    {
        static constexpr uint32_t k_invalid = 0xFFFFFFFF;
        uint32_t m_index = k_invalid;
        uint16_t m_generation = 0;

        bool valid() const
        {
            // 1) 無効値かどうかを判定する
            // 2) 有効なら true を返す
            return m_index != k_invalid;
        }
    };

    struct TransientTextureDesc final
    {
        D3D12_RESOURCE_DESC m_desc{};
        D3D12_CLEAR_VALUE m_clearValue{};
        bool m_useClearValue = false;
        bool m_allowRenderTarget = false;
        bool m_allowDepthStencil = false;
        bool m_allowUnorderedAccess = false;
    };

    struct TransientBufferDesc final
    {
        uint64_t m_size = 0;
        D3D12_RESOURCE_FLAGS m_flags = D3D12_RESOURCE_FLAG_NONE;
    };

    struct FrameGraphExecutionInfo final
    {
        ID3D12Fence* m_graphicsFence = nullptr;
        uint64_t m_graphicsFenceValue = 0;
    };

    class DRAMA_API FrameGraphPass
    {
    public:
        virtual ~FrameGraphPass() = default;
        virtual const char* get_name() const = 0;
        virtual PassType get_pass_type() const = 0;
        virtual void setup_static(FrameGraphBuilder& builder) = 0;
        virtual void update_imports(FrameGraphBuilder& builder)
        {
            // 1) 何もしないパスは空実装で済むようにする
            (void)builder;
        }
        virtual void pipeline_requests(PipelineRequestCollector& outRequests)
        {
            // 1) デフォルトは要求なしとして扱う
            (void)outRequests;
        }
        virtual void execute(FrameGraphContext& context) = 0;
        virtual bool allow_async_compute() const
        {
            // 1) 明示許可が無い場合は Graphics で動かす
            return false;
        }
        virtual bool allow_copy_queue() const
        {
            // 1) CopyQueue は明示許可制にする
            return false;
        }
    };

    class PipelineRequestCollector final
    {
    public:
        void add_request(const std::string& name)
        {
            // 1) 要求名を保持して後続のキャッシュ取得に使う
            m_requests.push_back(name);
        }
        const std::vector<std::string>& get_requests() const
        {
            // 1) 要求一覧を参照で返す
            return m_requests;
        }
        void clear()
        {
            // 1) 既存要求を破棄して次のフレームに備える
            m_requests.clear();
        }
    private:
        std::vector<std::string> m_requests;
    };

    class FrameGraph final
    {
    public:
        FrameGraph(
            DX12::RenderDevice& renderDevice,
            DX12::DescriptorAllocator& descriptorAllocator,
            DX12::CommandPool& commandPool);
        ~FrameGraph();

        void set_frames_in_flight(uint32_t framesInFlight);
        void set_async_compute_enabled(bool enabled);
        void set_copy_queue_enabled(bool enabled);
        void request_rebuild();

        void reset(uint64_t frameNo, uint32_t frameIndex);
        void add_pass(FrameGraphPass& pass);

        Core::Error::Result build();
        Core::Error::Result execute(FrameGraphExecutionInfo& outInfo);

        ResourceHandle create_transient_texture(const TransientTextureDesc& desc, const char* name);
        ResourceHandle create_transient_buffer(const TransientBufferDesc& desc, const char* name);
        ResourceHandle declare_imported_texture(const char* name);
        ResourceHandle declare_imported_buffer(const char* name);
        ResourceHandle import_texture(ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState,
            const DX12::DescriptorAllocator::TableID& rtvTable, const char* name);
        ResourceHandle import_buffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState, const char* name);
        void update_imported_texture(ResourceHandle handle, ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState,
            const DX12::DescriptorAllocator::TableID& rtvTable);
        void update_imported_buffer(ResourceHandle handle, ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState);

        void read_texture(ResourceHandle handle, D3D12_RESOURCE_STATES state, uint32_t passIndex);
        void write_texture(ResourceHandle handle, D3D12_RESOURCE_STATES state, uint32_t passIndex);
        void write_texture(ResourceHandle handle, D3D12_RESOURCE_STATES state, D3D12_RESOURCE_STATES finalState, uint32_t passIndex);
        void read_buffer(ResourceHandle handle, D3D12_RESOURCE_STATES state, uint32_t passIndex);
        void write_buffer(ResourceHandle handle, D3D12_RESOURCE_STATES state, uint32_t passIndex);
        void write_buffer(ResourceHandle handle, D3D12_RESOURCE_STATES state, D3D12_RESOURCE_STATES finalState, uint32_t passIndex);

        ID3D12Resource* get_resource(ResourceHandle handle);
        D3D12_CPU_DESCRIPTOR_HANDLE get_rtv(ResourceHandle handle) const;
        D3D12_CPU_DESCRIPTOR_HANDLE get_dsv(ResourceHandle handle) const;
        DX12::DescriptorAllocator::TableID get_srv_table(ResourceHandle handle);
        D3D12_GPU_DESCRIPTOR_HANDLE get_srv_gpu_handle(ResourceHandle handle);
        void export_texture(const char* name, ResourceHandle handle);
        void export_buffer(const char* name, ResourceHandle handle);
        ResourceHandle get_exported_texture(const char* name) const;
        ResourceHandle get_exported_buffer(const char* name) const;

        DX12::DescriptorAllocator& get_descriptor_allocator() const { return m_descriptorAllocator; }
        DX12::RenderDevice& get_render_device() const { return m_renderDevice; }
        uint64_t get_frame_no() const { return m_frameNo; }
        uint32_t get_frame_index() const { return m_frameIndex; }

    private:
        #ifndef NDEBUG
        class FrameGraphProfiler;
        #endif
        struct ResourceEntry;
        struct PassNode;
        struct ResourceAccess;
        struct PassExecutionInfo final
        {
            DX12::QueueContext* m_queue = nullptr;
            uint64_t m_fenceValue = 0;
        };

        DX12::QueueType select_queue(const PassNode& pass) const;
        void release_transient_descriptors();
        Core::Error::Result build_dependencies();
        Core::Error::Result build_execution_order();
        void reset_resource_states();
        Core::Error::Result execute_pass(uint32_t passIndex, FrameGraphExecutionInfo& outInfo,
            std::vector<PassExecutionInfo>& passExecution);
        void collect_barriers(const PassNode& pass, std::vector<D3D12_RESOURCE_BARRIER>& outBarriers);

        Core::Error::Result validate_handle(ResourceHandle handle, ResourceKind kind) const;

        DX12::RenderDevice& m_renderDevice;
        DX12::DescriptorAllocator& m_descriptorAllocator;
        DX12::CommandPool& m_commandPool;
        std::vector<PassNode> m_passes;
        std::vector<ResourceEntry> m_resources;
        std::vector<uint32_t> m_executionOrder;
        PipelineRequestCollector m_pipelineRequests;

        uint64_t m_frameNo = 0;
        uint32_t m_frameIndex = 0;
        uint32_t m_framesInFlight = 1;
        bool m_asyncComputeEnabled = false;
        bool m_copyQueueEnabled = false;
        bool m_forceRebuild = false;
        bool m_buildCacheValid = false;
        std::unordered_map<std::string, ResourceHandle> m_exportedResources;

        #ifndef NDEBUG
        std::unique_ptr<FrameGraphProfiler> m_profiler;
        #endif
    };

    class FrameGraphBuilder final
    {
    public:
        FrameGraphBuilder(FrameGraph& graph, uint32_t passIndex)
            : m_graph(graph), m_passIndex(passIndex)
        {
            // 1) FrameGraph と PassIndex を保持する
        }

        ResourceHandle create_transient_texture(const TransientTextureDesc& desc, const char* name);
        ResourceHandle create_transient_buffer(const TransientBufferDesc& desc, const char* name);
        ResourceHandle declare_imported_texture(const char* name);
        ResourceHandle declare_imported_buffer(const char* name);
        ResourceHandle import_texture(ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState,
            const DX12::DescriptorAllocator::TableID& rtvTable, const char* name);
        ResourceHandle import_buffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState, const char* name);
        void update_imported_texture(ResourceHandle handle, ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState,
            const DX12::DescriptorAllocator::TableID& rtvTable);
        void update_imported_buffer(ResourceHandle handle, ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState);
        void export_texture(const char* name, ResourceHandle handle);
        void export_buffer(const char* name, ResourceHandle handle);

        void read_texture(ResourceHandle handle, D3D12_RESOURCE_STATES state);
        void write_texture(ResourceHandle handle, D3D12_RESOURCE_STATES state);
        void write_texture(ResourceHandle handle, D3D12_RESOURCE_STATES state, D3D12_RESOURCE_STATES finalState);
        void read_buffer(ResourceHandle handle, D3D12_RESOURCE_STATES state);
        void write_buffer(ResourceHandle handle, D3D12_RESOURCE_STATES state);
        void write_buffer(ResourceHandle handle, D3D12_RESOURCE_STATES state, D3D12_RESOURCE_STATES finalState);

    private:
        FrameGraph& m_graph;
        uint32_t m_passIndex = 0;
    };

    class FrameGraphContext final
    {
    public:
        FrameGraphContext(
            FrameGraph& graph,
            ID3D12GraphicsCommandList* commandList,
            DX12::QueueType queueType)
            : m_graph(graph)
            , m_commandList(commandList)
            , m_queueType(queueType)
        {
            // 1) 実行時に必要な参照を保持する
        }

        ID3D12GraphicsCommandList* get_command_list() const { return m_commandList; }
        DX12::QueueType get_queue_type() const { return m_queueType; }
        ID3D12Resource* get_resource(ResourceHandle handle) { return m_graph.get_resource(handle); }
        D3D12_CPU_DESCRIPTOR_HANDLE get_rtv(ResourceHandle handle) const { return m_graph.get_rtv(handle); }
        D3D12_CPU_DESCRIPTOR_HANDLE get_dsv(ResourceHandle handle) const { return m_graph.get_dsv(handle); }
        DX12::DescriptorAllocator& get_descriptor_allocator() const { return m_graph.get_descriptor_allocator(); }
        DX12::RenderDevice& get_render_device() const { return m_graph.get_render_device(); }
        uint64_t get_frame_no() const { return m_graph.get_frame_no(); }
        uint32_t get_frame_index() const { return m_graph.get_frame_index(); }

    private:
        FrameGraph& m_graph;
        ID3D12GraphicsCommandList* m_commandList = nullptr;
        DX12::QueueType m_queueType = DX12::QueueType::Graphics;
    };
}

#include "pch.h"
#include "TransformWorldResource.h"

#include "Core/IO/public/LogAssert.h"
#include "GraphicsCore/public/RenderDevice.h"
#include "GraphicsCore/public/ResourceManager.h"

namespace Drama::Graphics
{
    namespace
    {
        constexpr uint32_t k_defaultTransformCapacity = 1024;
    }

    TransformWorldResource::~TransformWorldResource()
    {
        // 1) 明示破棄に委譲する
        destroy();
    }

    Core::Error::Result TransformWorldResource::initialize(
        DX12::RenderDevice& renderDevice,
        DX12::DescriptorAllocator& descriptorAllocator,
        DX12::ResourceManager& resourceManager,
        uint32_t framesInFlight)
    {
        // 1) 参照を保持して初期化条件を整える
        // 2) バッファを生成して使用可能にする
        (void)renderDevice;
        (void)descriptorAllocator;
        m_resourceManager = &resourceManager;
        m_framesInFlight = (framesInFlight == 0) ? 1 : framesInFlight;
        if (m_capacity == 0)
        {
            m_capacity = k_defaultTransformCapacity;
        }
        m_copyBytes = static_cast<uint64_t>(m_capacity) * sizeof(TransformData);

        Core::Error::Result result = create_buffers(m_framesInFlight);
        if (!result)
        {
            return result;
        }

        if (m_transformBufferMode == TransformBufferMode::DefaultWithStaging)
        {
            m_copyPass = std::make_unique<CopyPass>(*this);
        }
        return Core::Error::Result::ok();
    }

    void TransformWorldResource::destroy()
    {
        // 1) ディスクリプタを解放する
        // 2) バッファを破棄して参照を初期化する
        if (m_resourceManager)
        {
            for (auto& bufferId : m_uploadBufferIds)
            {
                m_resourceManager->destroy_gpu_buffer(bufferId);
            }
            for (auto& bufferId : m_defaultBufferIds)
            {
                m_resourceManager->destroy_gpu_buffer(bufferId);
            }
        }
        m_srvTables.clear();
        m_uploadBufferIds.clear();
        m_defaultBufferIds.clear();
        m_copyPass.reset();
        m_resourceManager = nullptr;
        m_framesInFlight = 1;
        m_capacity = 0;
        m_copyBytes = 0;
    }

    void TransformWorldResource::add_passes(FrameGraph& frameGraph)
    {
        // 1) CopyPass を追加してアップロードを反映する
        if (m_copyPass && m_transformBufferMode == TransformBufferMode::DefaultWithStaging)
        {
            frameGraph.add_pass(*m_copyPass);
        }
    }

    DX12::DescriptorAllocator::TableID TransformWorldResource::get_srv_table(uint32_t frameIndex) const
    {
        // 1) 範囲外は無効値を返す
        // 2) 対応する SRV テーブルを返す
        if (m_srvTables.empty())
        {
            return DX12::DescriptorAllocator::TableID{};
        }
        const uint32_t index = frameIndex % static_cast<uint32_t>(m_srvTables.size());
        return m_srvTables[index];
    }

    Core::Error::Result TransformWorldResource::create_buffers(uint32_t framesInFlight)
    {
        // 1) 必要数を確保して初期化する
        // 2) SRV テーブルを作成する
        if (!m_resourceManager)
        {
            return Core::Error::Result::fail(
                Core::Error::Facility::Graphics,
                Core::Error::Code::InvalidState,
                Core::Error::Severity::Error,
                0,
                "TransformWorldResource has invalid dependencies.");
        }
        m_uploadBufferIds.resize(framesInFlight);
        m_defaultBufferIds.clear();
        if (m_transformBufferMode == TransformBufferMode::DefaultWithStaging)
        {
            m_defaultBufferIds.resize(framesInFlight);
        }
        m_srvTables.resize(framesInFlight);

        for (uint32_t i = 0; i < framesInFlight; ++i)
        {
            uint32_t uploadIndex = m_resourceManager->create_upload_buffer<TransformData>(m_capacity, L"TransformUpload");
            DX12::DescriptorAllocator::TableID table{};
            if (m_transformBufferMode == TransformBufferMode::DefaultWithStaging)
            {
                uint32_t defaultIndex = m_resourceManager->create_structured_buffer<TransformData>(m_capacity, L"TransformDefault");
                table = m_resourceManager->create_srv_table(defaultIndex);
                m_defaultBufferIds[i] = defaultIndex;
            }
            else
            {
                table = m_resourceManager->create_srv_table(uploadIndex);
            }

            m_uploadBufferIds[i] = uploadIndex;
            m_srvTables[i] = table;
        }

        return Core::Error::Result::ok();
    }

    void TransformWorldResource::CopyPass::setup(FrameGraphBuilder& builder)
    {
        // 1) フレームに対応するバッファを import する
        // 2) Copy に必要な read/write 状態を宣言する
        const uint32_t index = m_owner.m_frameIndex % m_owner.m_framesInFlight;
        if (m_owner.m_defaultBufferIds.empty())
        {
            return;
        }
        auto* upload = m_owner.m_resourceManager->get_gpu_buffer(m_owner.m_uploadBufferIds[index]);
        auto* defaultBuffer = m_owner.m_resourceManager->get_gpu_buffer(m_owner.m_defaultBufferIds[index]);
        if (!upload || !defaultBuffer)
        {
            return;
        }

        m_src = builder.import_buffer(
            upload->get_resource(),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            "TransformUpload");
        m_dst = builder.import_buffer(
            defaultBuffer->get_resource(),
            D3D12_RESOURCE_STATE_COMMON,
            "TransformDefault");

        builder.read_buffer(m_src, D3D12_RESOURCE_STATE_GENERIC_READ);
        builder.write_buffer(m_dst, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
    }

    void TransformWorldResource::CopyPass::execute(FrameGraphContext& context)
    {
        // 1) CopyBufferRegion で Upload -> Default を反映する
        ID3D12GraphicsCommandList* commandList = context.get_command_list();
        if (commandList == nullptr)
        {
            return;
        }
        ID3D12Resource* src = context.get_resource(m_src);
        ID3D12Resource* dst = context.get_resource(m_dst);
        if (!src || !dst || m_owner.m_copyBytes == 0)
        {
            return;
        }
        commandList->CopyBufferRegion(dst, 0, src, 0, m_owner.m_copyBytes);
    }
}

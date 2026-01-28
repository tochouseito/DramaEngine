#include "pch.h"
#include "CameraViewResource.h"

#include "Core/IO/public/LogAssert.h"
#include "GraphicsCore/public/RenderDevice.h"
#include "GraphicsCore/public/ResourceManager.h"

namespace Drama::Graphics
{
    CameraViewResource::~CameraViewResource()
    {
        // 1) 明示破棄に委譲する
        destroy();
    }
    Core::Error::Result CameraViewResource::initialize(DX12::RenderDevice& renderDevice, DX12::DescriptorAllocator& descriptorAllocator, DX12::ResourceManager& resourceManager, uint32_t framesInFlight)
    {
        // 1) 参照を保持して初期化条件を整える
        // 2) バッファを生成して使用可能にする
        (void)renderDevice;
        (void)descriptorAllocator;
        m_resourceManager = &resourceManager;
        m_framesInFlight = (framesInFlight == 0) ? 1 : framesInFlight;
        m_copyBytes = sizeof(CameraData);

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
    void CameraViewResource::destroy()
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
        m_cbvTables.clear();
        m_uploadBufferIds.clear();
        m_defaultBufferIds.clear();
        m_copyPass.reset();
        m_resourceManager = nullptr;
        m_framesInFlight = 1;
        m_copyBytes = 0;
    }
    void CameraViewResource::add_passes(FrameGraph& frameGraph)
    {
        // 1) CopyPass を追加してアップロードを反映する
        if (m_copyPass && m_transformBufferMode == TransformBufferMode::DefaultWithStaging)
        {
            frameGraph.add_pass(*m_copyPass);
        }
    }
    DX12::DescriptorAllocator::TableID CameraViewResource::get_cbv_table(uint32_t frameIndex) const
    {
        // 1) 範囲外は無効値を返す
        // 2) 対応する SRV テーブルを返す
        if (m_cbvTables.empty())
        {
            return DX12::DescriptorAllocator::TableID{};
        }
        const uint32_t index = frameIndex % static_cast<uint32_t>(m_cbvTables.size());
        return m_cbvTables[index];
    }
    Core::Error::Result CameraViewResource::create_buffers(uint32_t framesInFlight)
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
                "CameraViewResource has invalid dependencies.");
        }
        m_uploadBufferIds.resize(framesInFlight);
        m_defaultBufferIds.clear();
        if (m_transformBufferMode == TransformBufferMode::DefaultWithStaging)
        {
            m_defaultBufferIds.resize(framesInFlight);
        }
        m_cbvTables.resize(framesInFlight);

        for (uint32_t i = 0; i < framesInFlight; ++i)
        {
            uint32_t uploadIndex = m_resourceManager->create_upload_buffer<CameraData>(1, L"CameraUpload");
            DX12::DescriptorAllocator::TableID table{};
            if (m_transformBufferMode == TransformBufferMode::DefaultWithStaging)
            {
                uint32_t defaultIndex = m_resourceManager->create_constant_buffer<CameraData>(L"CameraDefault");
                table = m_resourceManager->create_cbv_table(defaultIndex);
                m_defaultBufferIds[i] = defaultIndex;
            }
            else
            {
                table = m_resourceManager->create_srv_table(uploadIndex);
            }

            m_uploadBufferIds[i] = uploadIndex;
            m_cbvTables[i] = table;
        }

        return Core::Error::Result::ok();
    }

    void CameraViewResource::CopyPass::setup(FrameGraphBuilder& builder)
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
            "CameraUpload");
        m_dst = builder.import_buffer(
            defaultBuffer->get_resource(),
            D3D12_RESOURCE_STATE_COMMON,
            "CameraDefault");
        builder.read_buffer(m_src, D3D12_RESOURCE_STATE_GENERIC_READ);
        builder.write_buffer(m_dst, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
    }

    void CameraViewResource::CopyPass::execute(FrameGraphContext& context)
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

#include "pch.h"
#include "ForwardPassDepthViewResource.h"

#include "Core/IO/public/LogAssert.h"
#include "GraphicsCore/public/GraphicsConfig.h"
#include "GraphicsCore/public/RenderDevice.h"
#include "GraphicsCore/public/ResourceManager.h"

namespace Drama::Graphics
{
    ForwardPassDepthViewResource::~ForwardPassDepthViewResource()
    {
        // 1) 明示破棄に委譲する
        destroy();
    }

    Core::Error::Result ForwardPassDepthViewResource::initialize(
        DX12::ResourceManager& resourceManager,
        uint32_t framesInFlight)
    {
        // 1) 参照を保持して初期値を設定する
        // 2) 出力生成パスを用意する
        m_resourceManager = &resourceManager;
        m_renderDevice = &resourceManager.get_render_device();
        m_framesInFlight = (framesInFlight == 0) ? 1 : framesInFlight;

        m_width = (g_graphicsConfig.m_screenWidth == 0) ? 1 : g_graphicsConfig.m_screenWidth;
        m_height = (g_graphicsConfig.m_screenHeight == 0) ? 1 : g_graphicsConfig.m_screenHeight;
        m_depthFormat = g_graphicsConfig.m_depthStencilFormat;
        m_clearDepth = 1.0f;
        m_clearStencil = 0;

        if (!m_setupPass)
        {
            m_setupPass = std::make_unique<SetupPass>(*this);
        }
        m_requestRebuild = true;
        return Core::Error::Result::ok();
    }

    void ForwardPassDepthViewResource::destroy()
    {
        // 1) 参照をリセットして状態を初期化する
        // 2) パスとハンドルを破棄する
        if (m_descriptorAllocator && m_depthDsvTable.valid())
        {
            m_descriptorAllocator->free_table(m_depthDsvTable);
        }

        m_depthResource.Reset();
        m_depthDsvTable = {};
        m_depthHandle = {};
        m_setupPass.reset();
        m_resourceManager = nullptr;
        m_renderDevice = nullptr;
        m_descriptorAllocator = nullptr;
        m_framesInFlight = 1;
        m_requestRebuild = false;
    }

    void ForwardPassDepthViewResource::add_passes(FrameGraph& frameGraph)
    {
        // 1) 必要なら永続深度出力を再生成する
        // 2) 変更がある場合は再構築を要求する
        // 3) 出力生成パスを追加する
        if (!m_renderDevice)
        {
            return;
        }

        if (!m_depthResource || m_requestRebuild)
        {
            DX12::DescriptorAllocator& descriptorAllocator = frameGraph.get_descriptor_allocator();
            m_descriptorAllocator = &descriptorAllocator;
            if (m_depthDsvTable.valid())
            {
                descriptorAllocator.free_table(m_depthDsvTable);
                m_depthDsvTable = {};
            }

            m_depthResource.Reset();

            D3D12_RESOURCE_DESC resourceDesc{};
            resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            resourceDesc.Alignment = 0;
            resourceDesc.Width = m_width;
            resourceDesc.Height = m_height;
            resourceDesc.DepthOrArraySize = 1;
            resourceDesc.MipLevels = 1;
            resourceDesc.Format = m_depthFormat;
            resourceDesc.SampleDesc.Count = 1;
            resourceDesc.SampleDesc.Quality = 0;
            resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

            D3D12_CLEAR_VALUE clearValue{};
            clearValue.Format = m_depthFormat;
            clearValue.DepthStencil.Depth = m_clearDepth;
            clearValue.DepthStencil.Stencil = m_clearStencil;

            D3D12_HEAP_PROPERTIES heapProps{};
            heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

            HRESULT hr = m_renderDevice->get_d3d12_device()->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                D3D12_RESOURCE_STATE_COMMON,
                &clearValue,
                IID_PPV_ARGS(&m_depthResource));
            if (FAILED(hr))
            {
                Core::IO::LogAssert::assert_f(false, "ForwardPassDepthViewResource failed to create depth resource.");
                return;
            }

            DX12::SetD3D12Name(m_depthResource.Get(), L"ForwardPassDepth");

            m_depthDsvTable = descriptorAllocator.allocate(DX12::DescriptorAllocator::TableKind::DepthStencils);
            if (!m_depthDsvTable.valid())
            {
                Core::IO::LogAssert::assert_f(false, "ForwardPassDepthViewResource failed to allocate DSV table.");
                return;
            }
            descriptorAllocator.create_dsv(m_depthDsvTable, m_depthResource.Get());

            frameGraph.request_rebuild();
            m_requestRebuild = false;
        }

        if (m_setupPass)
        {
            frameGraph.add_pass(*m_setupPass);
        }
    }

    void ForwardPassDepthViewResource::SetupPass::setup_static(FrameGraphBuilder& builder)
    {
        // 1) 深度は外部リソースとして宣言する
        // 2) 書き込み状態と公開名を登録する
        m_owner.m_depthHandle = {};

        m_owner.m_depthHandle = builder.declare_imported_texture("ForwardPassDepth");
        builder.write_texture(
            m_owner.m_depthHandle,
            D3D12_RESOURCE_STATE_DEPTH_WRITE);
        builder.export_texture("ForwardPassDepth", m_owner.m_depthHandle);
    }

    void ForwardPassDepthViewResource::SetupPass::update_imports(FrameGraphBuilder& builder)
    {
        // 1) 永続深度出力の参照を反映する
        if (!m_owner.m_depthResource || !m_owner.m_depthDsvTable.valid())
        {
            return;
        }

        builder.update_imported_depth_texture(
            m_owner.m_depthHandle,
            m_owner.m_depthResource.Get(),
            D3D12_RESOURCE_STATE_COMMON,
            m_owner.m_depthDsvTable);
    }

    void ForwardPassDepthViewResource::SetupPass::execute(FrameGraphContext& context)
    {
        // 1) 描画コマンドを発行しない
        (void)context;
    }
}

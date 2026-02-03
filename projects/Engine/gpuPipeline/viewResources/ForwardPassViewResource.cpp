#include "pch.h"
#include "ForwardPassViewResource.h"

#include "Core/IO/public/LogAssert.h"
#include "GraphicsCore/public/GraphicsConfig.h"
#include "GraphicsCore/public/RenderDevice.h"
#include "GraphicsCore/public/ResourceManager.h"

namespace Drama::Graphics
{
    ForwardPassViewResource::~ForwardPassViewResource()
    {
        // 1) 明示破棄に委譲する
        destroy();
    }

    Core::Error::Result ForwardPassViewResource::initialize(
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
        m_colorFormat = g_graphicsConfig.m_ldrOffscreenFormat;
        m_clearColor = g_graphicsConfig.m_clearColor;

        if (!m_setupPass)
        {
            m_setupPass = std::make_unique<SetupPass>(*this);
        }
        m_requestRebuild = true;
        return Core::Error::Result::ok();
    }

    void ForwardPassViewResource::destroy()
    {
        // 1) 参照をリセットして状態を初期化する
        // 2) パスとハンドルを破棄する
        if (m_descriptorAllocator && m_colorRtvTable.valid())
        {
            m_descriptorAllocator->free_table(m_colorRtvTable);
        }

        m_colorResource.Reset();
        m_colorRtvTable = {};
        m_colorHandle = {};
        m_setupPass.reset();
        m_resourceManager = nullptr;
        m_renderDevice = nullptr;
        m_descriptorAllocator = nullptr;
        m_framesInFlight = 1;
        m_requestRebuild = false;
    }

    void ForwardPassViewResource::add_passes(FrameGraph& frameGraph)
    {
        // 1) 必要なら永続カラー出力を再生成する
        // 2) 変更がある場合は再構築を要求する
        // 3) 出力生成パスを追加する
        if (!m_renderDevice)
        {
            return;
        }

        if (!m_colorResource || m_requestRebuild)
        {
            DX12::DescriptorAllocator& descriptorAllocator = frameGraph.get_descriptor_allocator();
            m_descriptorAllocator = &descriptorAllocator;
            if (m_colorRtvTable.valid())
            {
                descriptorAllocator.free_table(m_colorRtvTable);
                m_colorRtvTable = {};
            }

            m_colorResource.Reset();

            D3D12_RESOURCE_DESC resourceDesc{};
            resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            resourceDesc.Alignment = 0;
            resourceDesc.Width = m_width;
            resourceDesc.Height = m_height;
            resourceDesc.DepthOrArraySize = 1;
            resourceDesc.MipLevels = 1;
            resourceDesc.Format = m_colorFormat;
            resourceDesc.SampleDesc.Count = 1;
            resourceDesc.SampleDesc.Quality = 0;
            resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

            D3D12_CLEAR_VALUE clearValue{};
            clearValue.Format = m_colorFormat;
            clearValue.Color[0] = m_clearColor[0];
            clearValue.Color[1] = m_clearColor[1];
            clearValue.Color[2] = m_clearColor[2];
            clearValue.Color[3] = m_clearColor[3];

            D3D12_HEAP_PROPERTIES heapProps{};
            heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

            HRESULT hr = m_renderDevice->get_d3d12_device()->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                D3D12_RESOURCE_STATE_COMMON,
                &clearValue,
                IID_PPV_ARGS(&m_colorResource));
            if (FAILED(hr))
            {
                Core::IO::LogAssert::assert_f(false, "ForwardPassViewResource failed to create color resource.");
                return;
            }

            DX12::SetD3D12Name(m_colorResource.Get(), L"ForwardPassColor");

            m_colorRtvTable = descriptorAllocator.allocate(DX12::DescriptorAllocator::TableKind::RenderTargets);
            if (!m_colorRtvTable.valid())
            {
                Core::IO::LogAssert::assert_f(false, "ForwardPassViewResource failed to allocate RTV table.");
                return;
            }
            descriptorAllocator.create_rtv(m_colorRtvTable, m_colorResource.Get());

            frameGraph.request_rebuild();
            m_requestRebuild = false;
        }

        if (m_setupPass)
        {
            frameGraph.add_pass(*m_setupPass);
        }
    }

    void ForwardPassViewResource::SetupPass::setup_static(FrameGraphBuilder& builder)
    {
        // 1) カラーは外部リソースとして宣言する
        // 2) 書き込み状態と公開名を登録する
        m_owner.m_colorHandle = {};

        m_owner.m_colorHandle = builder.declare_imported_texture("ForwardPassColor");
        builder.write_texture(
            m_owner.m_colorHandle,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        builder.export_texture("ForwardPassColor", m_owner.m_colorHandle);
    }

    void ForwardPassViewResource::SetupPass::update_imports(FrameGraphBuilder& builder)
    {
        // 1) 永続カラー出力の参照を反映する
        if (!m_owner.m_colorResource || !m_owner.m_colorRtvTable.valid())
        {
            return;
        }

        builder.update_imported_texture(
            m_owner.m_colorHandle,
            m_owner.m_colorResource.Get(),
            D3D12_RESOURCE_STATE_COMMON,
            m_owner.m_colorRtvTable);
    }

    void ForwardPassViewResource::SetupPass::execute(FrameGraphContext& context)
    {
        // 1) 描画コマンドを発行しない
        (void)context;
    }
}

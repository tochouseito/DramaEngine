#include "pch.h"
#include "GpuPipeline.h"
#include <array>
#include <Windows.h>

#include "Core/IO/public/LogAssert.h"
#include "Engine/gpuPipeline/worldResources/TransformWorldResource.h"
#include "GraphicsCore/public/DescriptorAllocator.h"
#include "GraphicsCore/public/GraphicsConfig.h"
#include "GraphicsCore/public/RenderDevice.h"
#include "GraphicsCore/public/SwapChain.h"
#include "GraphicsCore/public/ShaderCompiler.h"
#include "GraphicsCore/public/RootSignatureCache.h"
#include "GraphicsCore/public/PipelineStateCache.h"

namespace Drama::Graphics
{
    namespace
    {
        class BackBufferClearPass final : public FrameGraphPass
        {
        public:
            void set_backbuffer(
                ID3D12Resource* backBuffer,
                const DX12::DescriptorAllocator::TableID& rtvTable,
                uint32_t width,
                uint32_t height,
                const std::array<float, 4>& clearColor)
            {
                // 1) バックバッファ情報を保持する
                // 2) クリア条件を保持する
                m_backBuffer = backBuffer;
                m_rtvTable = rtvTable;
                m_width = width;
                m_height = height;
                m_clearColor = clearColor;
            }

            const char* get_name() const override
            {
                // 1) PIX 表示用の名前を返す
                return "BackBufferClear";
            }

            PassType get_pass_type() const override
            {
                // 1) GraphicsQueue で実行する
                return PassType::Render;
            }

            void setup(FrameGraphBuilder& builder) override
            {
                // 1) SwapChain バックバッファを import する
                // 2) RenderTarget -> Present の最終状態を指定する
                m_backBufferHandle = builder.import_texture(
                    m_backBuffer,
                    D3D12_RESOURCE_STATE_PRESENT,
                    m_rtvTable,
                    "BackBuffer");
                builder.write_texture(
                    m_backBufferHandle,
                    D3D12_RESOURCE_STATE_RENDER_TARGET,
                    D3D12_RESOURCE_STATE_PRESENT);
            }

            void execute(FrameGraphContext& context) override
            {
                // 1) バックバッファをクリアする
                // 2) 次の Present に備える
                ID3D12GraphicsCommandList* commandList = context.get_command_list();
                if (commandList == nullptr)
                {
                    return;
                }

                ID3D12DescriptorHeap* heaps[] = {
                    context.get_descriptor_allocator().get_descriptor_heap(DX12::HeapType::CBV_SRV_UAV)
                };
                commandList->SetDescriptorHeaps(_countof(heaps), heaps);

                D3D12_VIEWPORT viewport{};
                viewport.TopLeftX = 0.0f;
                viewport.TopLeftY = 0.0f;
                viewport.Width = static_cast<float>(m_width);
                viewport.Height = static_cast<float>(m_height);
                viewport.MinDepth = 0.0f;
                viewport.MaxDepth = 1.0f;
                commandList->RSSetViewports(1, &viewport);

                D3D12_RECT scissorRect{};
                scissorRect.left = 0;
                scissorRect.top = 0;
                scissorRect.right = static_cast<LONG>(m_width);
                scissorRect.bottom = static_cast<LONG>(m_height);
                commandList->RSSetScissorRects(1, &scissorRect);

                D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = context.get_rtv(m_backBufferHandle);
                commandList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);
                commandList->ClearRenderTargetView(rtvHandle, m_clearColor.data(), 0, nullptr);
            }

        private:
            ID3D12Resource* m_backBuffer = nullptr;
            DX12::DescriptorAllocator::TableID m_rtvTable{};
            ResourceHandle m_backBufferHandle{};
            uint32_t m_width = 0;
            uint32_t m_height = 0;
            std::array<float, 4> m_clearColor{};
        };
    }

    GpuPipeline::GpuPipeline(DX12::RenderDevice& renderDevice, DX12::DescriptorAllocator& descriptorAllocator, DX12::ResourceManager& resourceManager, DX12::SwapChain& swapChain, DX12::CommandPool& commandPool, DX12::ShaderCompiler& shaderCompiler, DX12::RootSignatureCache& rootSignatureCache, DX12::PipelineStateCache& pipelineStateCache, GpuPipelineDesc desc)
        : m_renderDevice(renderDevice)
        , m_descriptorAllocator(descriptorAllocator)
        , m_resourceManager(resourceManager)
        , m_swapChain(swapChain)
        , m_commandPool(commandPool)
        , m_shaderCompiler(shaderCompiler)
        , m_rootSignatureCache(rootSignatureCache)
        , m_pipelineStateCache(pipelineStateCache)
        , m_frameGraph(renderDevice, descriptorAllocator, commandPool)
        , m_desc(desc)
    {
        // 1) 設定値を正規化する
        // 2) FrameGraph の初期設定を行う
        if (m_desc.m_framesInFlight < 1)
        {
            m_desc.m_framesInFlight = 1;
        }
        if (m_desc.m_framesInFlight > 3)
        {
            m_desc.m_framesInFlight = 3;
        }

        if (m_desc.m_framesInFlight == 1)
        {
            Core::IO::LogAssert::log("FramesInFlight=1 はデバッグ用途として扱います。");
        }
        if (m_desc.m_transformBufferMode == TransformBufferMode::UploadOnly)
        {
            Core::IO::LogAssert::log("TransformBufferMode=UploadOnly は性能注意モードです。");
        }

        m_frameGraph.set_frames_in_flight(m_desc.m_framesInFlight);
        m_frameGraph.set_async_compute_enabled(m_desc.m_enableAsyncCompute);
        m_frameGraph.set_copy_queue_enabled(m_desc.m_enableCopyQueue);

        m_frameFenceValues.assign(m_desc.m_framesInFlight, 0);
        m_defaultPass = std::make_unique<BackBufferClearPass>();

        m_transformWorldResource = std::make_unique<TransformWorldResource>();
        m_transformWorldResource->set_transform_buffer_mode(m_desc.m_transformBufferMode);
        m_transformWorldResource->set_capacity(m_desc.m_transformBufferCapacity);
        Core::Error::Result initResult = m_transformWorldResource->initialize(
            m_resourceManager,
            m_desc.m_framesInFlight);
        Core::IO::LogAssert::assert_f(initResult, "TransformWorldResource initialize failed.");
        m_worldResources.push_back(m_transformWorldResource.get());

        // demo pso 作成
        DX12::ShaderCompileDesc vsDesc{};
        vsDesc.name = L"DemoVertexShader";
        vsDesc.enableDebugInfo = true;
        vsDesc.entryPoint = L"VSMain";
        vsDesc.filePath = L"shader/demo.vs.hlsl";
        vsDesc.targetProfile = L"vs_" + DX12::shader_profile_to_wstring(g_graphicsConfig.m_highestShaderModel);
        DX12::ComPtr<IDxcBlob> vsBlob = m_shaderCompiler.compile_shader_raw(vsDesc);
        DX12::ShaderCompileDesc psDesc{};
        psDesc.name = L"DemoPixelShader";
        psDesc.enableDebugInfo = true;
        psDesc.entryPoint = L"PSMain";
        psDesc.filePath = L"shader/demo.ps.hlsl";
        psDesc.targetProfile = L"ps_" + DX12::shader_profile_to_wstring(g_graphicsConfig.m_highestShaderModel);
        DX12::ComPtr<IDxcBlob> psBlob = m_shaderCompiler.compile_shader_raw(psDesc);
        DX12::RootSignatureDesc rootSigDesc{};
        std::vector<DX12::RootParameterDesc> params;
        DX12::RootParameterDesc param0{};
        // ViewProjection CBV
        param0.type = DX12::RootParameterType::CBV;
        param0.visibility = DX12::ShaderVisibility::Vertex;
        param0.shaderRegister = 0;
        params.push_back(param0);
        // Object id
        param0.type = DX12::RootParameterType::_32BitConstants;
        param0.visibility = DX12::ShaderVisibility::Vertex;
        param0.shaderRegister = 1;
        params.push_back(param0);
        // object
        param0.type = DX12::RootParameterType::SRV;
        param0.visibility = DX12::ShaderVisibility::Vertex;
        param0.shaderRegister = 0;
        params.push_back(param0);
        // transform
        param0.type = DX12::RootParameterType::SRV;
        param0.visibility = DX12::ShaderVisibility::Vertex;
        param0.shaderRegister = 1;
        params.push_back(param0);
        rootSigDesc.parameters = params;
        Core::Error::Result rsResult = m_rootSignatureCache.create("DemoRootSignature", rootSigDesc);
        Core::IO::LogAssert::assert_f(rsResult, "Failed to create DemoRootSignature.");
        DX12::ComPtr<ID3D12RootSignature> rootSignature;
        Core::Error::Result getRsResult = m_rootSignatureCache.get("DemoRootSignature", rootSignature);
        Core::IO::LogAssert::assert_f(getRsResult, "Failed to get DemoRootSignature.");
        DX12::GraphicsPipelineStateDesc psoDesc{};
        psoDesc.vertexShaderName = "DemoVertexShader";
        psoDesc.pixelShaderName = "DemoPixelShader";
        psoDesc.rootSignatureName = "DemoRootSignature";
        DX12::InputElementDesc inputElem0{};
        inputElem0.semanticName = "POSITION";
        inputElem0.semanticIndex = 0;
        inputElem0.format = DX12::InputElementFormat::R32G32B32A32_Float;
        psoDesc.inputElements.push_back(inputElem0);
        DX12::InputElementDesc inputElem1{};
        inputElem1.semanticName = "NORMAL";
        inputElem1.semanticIndex = 0;
        inputElem1.format = DX12::InputElementFormat::R32G32B32_Float;
        psoDesc.inputElements.push_back(inputElem1);
        DX12::InputElementDesc inputElem2{};
        inputElem2.semanticName = "TEXCOORD";
        inputElem2.semanticIndex = 0;
        inputElem2.format = DX12::InputElementFormat::R32G32_Float;
        psoDesc.inputElements.push_back(inputElem2);
        DX12::RasterizerStateDesc rasterDesc{};
        rasterDesc.cullMode = DX12::CullMode::Back;
        rasterDesc.fillMode = DX12::FillMode::Solid;
        psoDesc.rasterizerState = rasterDesc;
        DX12::DepthStencilStateDesc depthDesc{};
        depthDesc.depthEnable = true;
        depthDesc.depthWriteMask = DX12::DepthWriteMask::All;
        depthDesc.depthFunc = DX12::ComparisonFunc::LessEqual;
        psoDesc.depthStencilState = depthDesc;
        psoDesc.blendMode.push_back(DX12::BlendMode::None);
        psoDesc.dsvFormat = DX12::DSVFormat::D24_UNorm_S8_UInt;
        psoDesc.primitiveTopologyType = DX12::PrimitiveTopologyType::Triangle;
        psoDesc.rtvFormats.push_back(DX12::RTVFormat::R8G8B8A8_UNorm);
        Core::Error::Result psoResult = m_pipelineStateCache.create_graphics("DemoPipelineState", psoDesc);
    }

    GpuPipeline::~GpuPipeline()
    {
        // 1) 永続リソースを安全に破棄する
        if (m_transformWorldResource)
        {
            m_transformWorldResource->destroy();
        }
    }

    void GpuPipeline::register_pass(std::unique_ptr<FrameGraphPass> pass)
    {
        // 1) nullptr を弾いて登録する
        if (!pass)
        {
            return;
        }
        m_registeredPasses.push_back(std::move(pass));
    }

    void GpuPipeline::clear_registered_passes()
    {
        // 1) 登録済みパスを破棄する
        m_registeredPasses.clear();
    }

    void GpuPipeline::wait_for_frame(uint32_t index)
    {
        // 1) フェンス未設定なら待機しない
        // 2) 完了していなければイベント待機する
        if (m_graphicsFence == nullptr || m_frameFenceValues.empty())
        {
            return;
        }
        const uint32_t frameIndex = index % static_cast<uint32_t>(m_frameFenceValues.size());
        const uint64_t fenceValue = m_frameFenceValues[frameIndex];
        if (fenceValue == 0)
        {
            return;
        }
        if (m_graphicsFence->GetCompletedValue() < fenceValue)
        {
            HANDLE eventHandle = CreateEvent(nullptr, false, false, nullptr);
            if (eventHandle)
            {
                m_graphicsFence->SetEventOnCompletion(fenceValue, eventHandle);
                WaitForSingleObject(eventHandle, INFINITE);
                CloseHandle(eventHandle);
            }
        }
    }
    void GpuPipeline::render(uint64_t frameNo, uint32_t index)
    {
        // 1) FramePipeline の index をリソース用に使って同期する
        // 2) SwapChain の backbuffer は別途取得して描画対象にする
        const uint32_t resourceIndex = index;
        const uint32_t backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
        wait_for_frame(resourceIndex);

        DX12::ComPtr<ID3D12Resource> backBuffer;
        HRESULT hr = m_swapChain->GetBuffer(backBufferIndex, IID_PPV_ARGS(&backBuffer));
        if (FAILED(hr))
        {
            Core::IO::LogAssert::assert_f(false, "Failed to get SwapChain back buffer.");
            return;
        }

        uint32_t frameIndex = 0;
        if (!m_frameFenceValues.empty())
        {
            frameIndex = resourceIndex % static_cast<uint32_t>(m_frameFenceValues.size());
        }
        m_frameGraph.reset(frameNo, frameIndex);

        for (auto* resource : m_worldResources)
        {
            if (resource)
            {
                resource->set_frame_index(resourceIndex);
                resource->add_passes(m_frameGraph);
            }
        }

        if (m_registeredPasses.empty())
        {
            auto* clearPass = static_cast<BackBufferClearPass*>(m_defaultPass.get());
            if (clearPass)
            {
                clearPass->set_backbuffer(
                    backBuffer.Get(),
                    m_swapChain.get_rtv_table(backBufferIndex),
                    g_graphicsConfig.m_screenWidth,
                    g_graphicsConfig.m_screenHeight,
                    g_graphicsConfig.m_clearColor);
                m_frameGraph.add_pass(*clearPass);
            }
        }
        else
        {
            for (auto& pass : m_registeredPasses)
            {
                if (pass)
                {
                    m_frameGraph.add_pass(*pass);
                }
            }
        }

        Core::Error::Result buildResult = m_frameGraph.build();
        if (!buildResult)
        {
            Core::IO::LogAssert::log("FrameGraph build failed: {}", buildResult.message);
            return;
        }

        FrameGraphExecutionInfo execInfo{};
        Core::Error::Result execResult = m_frameGraph.execute(execInfo);
        if (!execResult)
        {
            Core::IO::LogAssert::log("FrameGraph execute failed: {}", execResult.message);
            return;
        }

        if (execInfo.m_graphicsFence != nullptr && !m_frameFenceValues.empty())
        {
            m_graphicsFence = execInfo.m_graphicsFence;
            m_frameFenceValues[frameIndex] = execInfo.m_graphicsFenceValue;
        }
    }
    void GpuPipeline::present(uint64_t frameNo, uint32_t index)
    {
        frameNo; index;
        // 1) Present を実行して表示を更新する
        if (g_graphicsConfig.m_enableVSync)
        {
            // VSync有効時
            m_swapChain->Present(1, 0);
        }
        else
        {
            // VSync無効時
            m_swapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
        }
    }
}

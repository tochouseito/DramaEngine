#include "pch.h"
#include "PipelineManager.h"
#include "GraphicsConfig.h"
#include "RenderDevice.h"
#include "ShaderCompiler.h"
#include "Core/IO/public/LogAssert.h"

namespace Drama::Graphics::DX12
{
    void PipelineManager::create_default_pipelines()
    {
        GraphicsPipelineSettings defaultPipelineSettings{};
        defaultPipelineSettings.name = "DefaultPipeline";
        create_graphics_pipeline(defaultPipelineSettings);
        m_graphicsPipelineNameToIndex[defaultPipelineSettings.name] = m_graphicsPipelineSettings.size();
        m_graphicsPipelineSettings.emplace_back(std::move(defaultPipelineSettings));
    }
    void PipelineManager::create_graphics_pipeline(GraphicsPipelineSettings& settings)
    {
        HRESULT hr = S_OK;

        // ルートシグネチャの作成
        D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
        rootSignatureDesc.Flags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        // ルートパラメータの作成
        std::vector<D3D12_ROOT_PARAMETER> rootParms;// ルートパラメータ配列
        std::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplers;// スタティックサンプラ配列
        // D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};// 入力レイアウト
        ComPtr<IDxcBlob> vsBlob = nullptr;
        ComPtr<IDxcBlob> psBlob = nullptr;

        // シェーダのコンパイル
        // vs
        if (!settings.vs.empty())
        {
            ShaderCompileDesc vsCompileDesc{};
            vsCompileDesc.filePath = L"shader/";
            vsCompileDesc.entryPoint = L"VSMain";
            vsCompileDesc.targetProfile = L"vs_6_0";
#ifndef NDEBUG
            vsCompileDesc.enableDebugInfo = true;
#else
            vsCompileDesc.enableDebugInfo = false;
#endif
            vsBlob = m_shaderCompiler.get_compile_shader(vsCompileDesc);
        }

        // ps
        if (!settings.ps.empty())
        {
            ShaderCompileDesc psCompileDesc{};
            psCompileDesc.filePath = L"shader/";
            psCompileDesc.entryPoint = L"PSMain";
            psCompileDesc.targetProfile = L"ps_6_0";
#ifndef NDEBUG
            psCompileDesc.enableDebugInfo = true;
#else
            psCompileDesc.enableDebugInfo = false;
#endif
            psBlob = m_shaderCompiler.get_compile_shader(psCompileDesc);
        }

        rootParms.resize(2, {});
        rootParms[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParms[0].Descriptor.ShaderRegister = 0;
        rootParms[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;


        // InputLayout
        D3D12_INPUT_ELEMENT_DESC inputElementDesc[1] = {};
        inputElementDesc[0].SemanticName = "POSITION";
        inputElementDesc[0].SemanticIndex = 0;
        inputElementDesc[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        inputElementDesc[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
        D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
        inputLayoutDesc.pInputElementDescs = inputElementDesc;
        inputLayoutDesc.NumElements = _countof(inputElementDesc);

        // RasterizerStateの設定
        D3D12_RASTERIZER_DESC rasterizerDesc = {};
        rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;// 塗りつぶし
        rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;// 裏面カリング

        // DepthStencilStateの設定
        D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
        depthStencilDesc.DepthEnable = true;// 深度有効
        depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;// 書き込み許可
        depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;// 近ければ描画

        // PipelineStateDescの設定
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = {};
        pipelineDesc.pRootSignature = settings.rootSignature.Get();
        pipelineDesc.VS = { vsBlob->GetBufferPointer(),vsBlob->GetBufferSize() };
        pipelineDesc.PS = { psBlob->GetBufferPointer(),psBlob->GetBufferSize() };
        pipelineDesc.InputLayout = inputLayoutDesc;
        pipelineDesc.RasterizerState = rasterizerDesc;
        pipelineDesc.DepthStencilState = depthStencilDesc;
        pipelineDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pipelineDesc.NumRenderTargets = 1;
        pipelineDesc.RTVFormats[0] = graphicsConfig.m_ldrOffscreenFormat;

        for (size_t i = 0; i < static_cast<size_t>(BlendMode::kCount); ++i)
        {
            // BlendStateの設定
            /*
            out.rgb = src.rgb * SrcBlend     + dst.rgb * DestBlend
            out.a   = src.a   * SrcBlendAlpha + dst.a   * DestBlendAlpha
            */
            D3D12_BLEND_DESC blendDesc = {};
            switch (static_cast<BlendMode>(i))
            {
            case BlendMode::None:// out = src * 1 + dst * 0 = src
                blendDesc.RenderTarget[0].BlendEnable = false;
                blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
                blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
                blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
                blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
                blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
                blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
                blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
                break;
            case BlendMode::Normal:// out.rgb = src.rgb * src.a + dst.rgb * (1 - src.a)
                blendDesc.RenderTarget[0].BlendEnable = true;

                // 色
                blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
                blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
                blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;

                // アルファ（だいたい同じでいい）
                blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
                blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
                blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
                break;
            case BlendMode::Add:// out.rgb = src.rgb * src.a + dst.rgb
                blendDesc.RenderTarget[0].BlendEnable = true;
                blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;  // src * alpha
                blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;        // dst * 1
                blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;

                // αはとりあえずそのまま足すか、dst を維持するかは好み
                blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
                blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
                blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
                break;
            case BlendMode::Subtract:// out.rgb = dst.rgb - src.rgb * src.a
                blendDesc.RenderTarget[0].BlendEnable = true;
                blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;      // src * src.a
                blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;            // dst * 1
                blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
                // out = dst*1 - src*src.a

                blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
                blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
                blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD; // αは適当でOKなことが多い
                break;
            case BlendMode::Multiply:// out = src * dst + dst * 0 = src * dst
                blendDesc.RenderTarget[0].BlendEnable = true;
                blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_DEST_COLOR; // src * dst
                blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;       // dst * 0
                blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
                break;
            case BlendMode::Screen:// out.rgb = src.rgb * 1 + dst.rgb * (1 - src.rgb) = src + dst - src * dst
                blendDesc.RenderTarget[0].BlendEnable = true;
                blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;         // src * 1
                blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_COLOR; // dst * (1 - src)
                blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;

                // アルファはお好み
                blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
                blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
                blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
                break;
            default:
                break;
            }

            pipelineDesc.BlendState = blendDesc;

            // CreatePSO
            hr = m_renderDevice.get_d3d12_device()->CreateGraphicsPipelineState(
                &pipelineDesc, IID_PPV_ARGS(&settings.pso[i]));
            // 生成できたかチェック 失敗ならアサート
            if (FAILED(hr))
            {
                Core::IO::LogAssert::assert(false, "Failed Create GraphicsPipelineState!!");
            }
        }
    }
}// namespace Drama::Graphics::DX12

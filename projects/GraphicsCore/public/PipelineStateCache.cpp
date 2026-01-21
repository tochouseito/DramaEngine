#include "pch.h"
#include "PipelineStateCache.h"

#include "RenderDevice.h"
#include "RootSignatureCache.h"
#include "ShaderCompiler.h"

#include <cstring>

namespace
{
    D3D12_INPUT_ELEMENT_DESC convert_input_element_desc(const Drama::Graphics::DX12::InputElementDesc& desc)
    {
        using namespace Drama::Graphics::DX12;
        D3D12_INPUT_ELEMENT_DESC d3dDesc{};
        d3dDesc.SemanticName = desc.semanticName.c_str();
        d3dDesc.SemanticIndex = desc.semanticIndex;
        switch (desc.format)
        {
        case InputElementFormat::R32G32B32A32_Float:
            d3dDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            break;
        case InputElementFormat::R32G32B32_Float:
            d3dDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
            break;
        case InputElementFormat::R32G32_Float:
            d3dDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
            break;
        case InputElementFormat::R32_Float:
            d3dDesc.Format = DXGI_FORMAT_R32_FLOAT;
            break;
        default:
            d3dDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            break;
        }
        d3dDesc.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
        return d3dDesc;
    }

    D3D12_BLEND_DESC convert_blend_mode(const std::vector<Drama::Graphics::DX12::BlendMode>& modes)
    {
        using namespace Drama::Graphics::DX12;
        D3D12_BLEND_DESC desc{};
        for (size_t i = 0; i < modes.size() && i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        {
            D3D12_RENDER_TARGET_BLEND_DESC& rtDesc = desc.RenderTarget[i];
            switch (modes[i])
            {
            case BlendMode::None:
                rtDesc.BlendEnable = false;
                break;
            case BlendMode::Normal:
                rtDesc.BlendEnable = true;
                break;
            default:
                rtDesc.BlendEnable = false;
                break;
            }
        }
        return desc;
    }

    D3D12_RASTERIZER_DESC convert_rasterizer_state(const Drama::Graphics::DX12::RasterizerStateDesc& desc)
    {
        using namespace Drama::Graphics::DX12;
        D3D12_RASTERIZER_DESC d3dDesc{};
        switch (desc.cullMode)
        {
        case CullMode::None:
            d3dDesc.CullMode = D3D12_CULL_MODE_NONE;
            break;
        case CullMode::Front:
            d3dDesc.CullMode = D3D12_CULL_MODE_FRONT;
            break;
        case CullMode::Back:
            d3dDesc.CullMode = D3D12_CULL_MODE_BACK;
            break;
        default:
            d3dDesc.CullMode = D3D12_CULL_MODE_BACK;
            break;
        }
        switch (desc.fillMode)
        {
        case FillMode::Solid:
            d3dDesc.FillMode = D3D12_FILL_MODE_SOLID;
            break;
        case FillMode::Wireframe:
            d3dDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
            break;
        default:
            d3dDesc.FillMode = D3D12_FILL_MODE_SOLID;
            break;
        }
        return d3dDesc;
    }

    D3D12_DEPTH_STENCIL_DESC convert_depth_stencil_state(const Drama::Graphics::DX12::DepthStencilStateDesc& desc)
    {
        using namespace Drama::Graphics::DX12;
        D3D12_DEPTH_STENCIL_DESC d3dDesc{};
        d3dDesc.DepthEnable = desc.depthEnable ? TRUE : FALSE;
        d3dDesc.DepthWriteMask = (desc.depthWriteMask == DepthWriteMask::All) ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        switch (desc.depthFunc)
        {
        case ComparisonFunc::LessEqual:
            d3dDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
            break;
        default:
            d3dDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
            break;
        }
        return d3dDesc;
    }

    DXGI_FORMAT convert_dsv_format(Drama::Graphics::DX12::DSVFormat format)
    {
        using namespace Drama::Graphics::DX12;
        switch (format)
        {
        case DSVFormat::D24_UNorm_S8_UInt:
            return DXGI_FORMAT_D24_UNORM_S8_UINT;
        default:
            return DXGI_FORMAT_D24_UNORM_S8_UINT;
        }
    }

    D3D12_PRIMITIVE_TOPOLOGY_TYPE convert_primitive_topology_type(Drama::Graphics::DX12::PrimitiveTopologyType type)
    {
        using namespace Drama::Graphics::DX12;
        switch (type)
        {
        case PrimitiveTopologyType::Triangle:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        default:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        }
    }

    DXGI_FORMAT convert_rtv_format(Drama::Graphics::DX12::RTVFormat format)
    {
        using namespace Drama::Graphics::DX12;
        switch (format)
        {
        case RTVFormat::R8G8B8A8_UNorm:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case RTVFormat::R8G8B8A8_UNorm_SRGB:
            return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        default:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        }
    }
}

namespace Drama::Graphics::DX12
{
    Result PipelineStateCache::create_graphics(const std::string& name, const GraphicsPipelineStateDesc& desc)
    {
        using namespace Drama::Graphics::DX12;
        using namespace Drama::Core::Error;
        HRESULT hr = S_OK;
        std::lock_guard lock(m_mutex);
        // 1) ルートシグネチャを取得する
        ComPtr<ID3D12RootSignature> rootSignature;
        {
            Result r = m_rootSignatureCache.get(desc.rootSignatureName, rootSignature);
            if (!r)
            {
                return r;
            }
        }
        // 2) シェーダーを取得する
        ComPtr<IDxcBlob> vertexShaderBlob;
        {
            Result r = m_shaderCompiler.get_shader_blob(desc.vertexShaderName, vertexShaderBlob);
            if (!r)
            {
                return r;
            }
        }
        ComPtr<IDxcBlob> pixelShaderBlob;
        {
            Result r = m_shaderCompiler.get_shader_blob(desc.pixelShaderName, pixelShaderBlob);
            if (!r)
            {
                return r;
            }
        }
        // 3) 入力レイアウトを変換する
        std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescs;
        for (const auto& elem : desc.inputElements)
        {
            inputElementDescs.push_back(convert_input_element_desc(elem));
        }
        D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
        inputLayoutDesc.pInputElementDescs = inputElementDescs.data();
        inputLayoutDesc.NumElements = static_cast<UINT>(inputElementDescs.size());
        // 4) ブレンドステートを変換する
        D3D12_BLEND_DESC blendDesc = convert_blend_mode(desc.blendMode);
        // 5) ラスタライザーステートを変換する
        D3D12_RASTERIZER_DESC rasterizerDesc = convert_rasterizer_state(desc.rasterizerState);
        // 6) デプスステンシルステートを変換する
        D3D12_DEPTH_STENCIL_DESC depthStencilDesc = convert_depth_stencil_state(desc.depthStencilState);
        // 7) PSO 設定を構築する
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
        psoDesc.pRootSignature = rootSignature.Get();
        psoDesc.VS.pShaderBytecode = vertexShaderBlob->GetBufferPointer();
        psoDesc.VS.BytecodeLength = vertexShaderBlob->GetBufferSize();
        psoDesc.PS.pShaderBytecode = pixelShaderBlob->GetBufferPointer();
        psoDesc.PS.BytecodeLength = pixelShaderBlob->GetBufferSize();
        psoDesc.InputLayout = inputLayoutDesc;
        psoDesc.BlendState = blendDesc;
        psoDesc.RasterizerState = rasterizerDesc;
        psoDesc.DepthStencilState = depthStencilDesc;
        psoDesc.DSVFormat = convert_dsv_format(desc.dsvFormat);
        psoDesc.PrimitiveTopologyType = convert_primitive_topology_type(desc.primitiveTopologyType);
        psoDesc.NumRenderTargets = static_cast<UINT>(desc.rtvFormats.size());
        for (size_t i = 0; i < desc.rtvFormats.size() && i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        {
            psoDesc.RTVFormats[i] = convert_rtv_format(desc.rtvFormats[i]);
        }
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
        // 8) PSO を生成する
        ComPtr<ID3D12PipelineState> pipelineState;
        hr = m_renderDevice.get_d3d12_device()->CreateGraphicsPipelineState(
            &psoDesc,
            IID_PPV_ARGS(&pipelineState)
        );
        if (FAILED(hr))
        {
            return Result::fail(
                Facility::D3D12,
                Code::CreationFailed,
                Severity::Error,
                static_cast<uint32_t>(hr),
                "Failed to create graphics pipeline state object."
            );
        }
        // 9) キャッシュに登録する
        uint32_t index = static_cast<uint32_t>(m_graphicsCache.size());
        m_graphicsCache.push_back(pipelineState);
        m_nameToGraphicsCacheIndex[name] = index;
        return Result::ok();
    }
    Result PipelineStateCache::get_graphics(const std::string& name, ComPtr<ID3D12PipelineState>& outPipelineState)
    {
        return Result();
    }
    void PipelineStateCache::clear()
    {
        std::lock_guard lock(m_mutex);
        m_graphicsCache.clear();
        m_nameToGraphicsCacheIndex.clear();
        m_computeCache.clear();
        m_nameToComputeCacheIndex.clear();
    }
}

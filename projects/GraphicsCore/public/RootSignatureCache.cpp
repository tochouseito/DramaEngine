#include "pch.h"
#include "RootSignatureCache.h"

#include "RenderDevice.h"

namespace
{
    D3D12_ROOT_PARAMETER_TYPE convert_root_parameter_type(Drama::Graphics::DX12::RootParameterType type)
    {
        using namespace Drama::Graphics::DX12;
        switch (type)
        {
        case RootParameterType::CBV:
            return D3D12_ROOT_PARAMETER_TYPE_CBV;
        case RootParameterType::SRV:
            return D3D12_ROOT_PARAMETER_TYPE_SRV;
        default:
            return D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        }
    }

    D3D12_SHADER_VISIBILITY convert_shader_visibility(Drama::Graphics::DX12::ShaderVisibility visibility)
    {
        using namespace Drama::Graphics::DX12;
        switch (visibility)
        {
        case ShaderVisibility::All:
            return D3D12_SHADER_VISIBILITY_ALL;
        case ShaderVisibility::Vertex:
            return D3D12_SHADER_VISIBILITY_VERTEX;
        case ShaderVisibility::Pixel:
            return D3D12_SHADER_VISIBILITY_PIXEL;
        default:
            return D3D12_SHADER_VISIBILITY_ALL;
        }
    }
}

namespace Drama::Graphics::DX12
{
    Result RootSignatureCache::create(std::string_view name, const RootSignatureDesc& desc)
    {
        std::lock_guard lock(m_mutex);
        D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        std::vector<D3D12_ROOT_PARAMETER> d3dParameters;
        d3dParameters.reserve(desc.parameters.size());
        for (auto parmDesc : desc.parameters)
        {
            D3D12_ROOT_PARAMETER d3dParam{};
            d3dParam.ParameterType = convert_root_parameter_type(parmDesc.type);
            d3dParam.ShaderVisibility = convert_shader_visibility(parmDesc.visibility);
            d3dParam.Descriptor.ShaderRegister = parmDesc.shaderRegister;
            d3dParam.Descriptor.RegisterSpace = 0;
            d3dParameters.push_back(d3dParam);
        }
        rootSignatureDesc.NumParameters = static_cast<UINT>(d3dParameters.size());
        rootSignatureDesc.pParameters = d3dParameters.data();

        // Serialize and create the root signature
        ComPtr<ID3DBlob> serializedRootSig;
        ComPtr<ID3DBlob> errorBlob;
        HRESULT hr = D3D12SerializeRootSignature(
            &rootSignatureDesc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            &serializedRootSig,
            &errorBlob);
        if (FAILED(hr))
        {
            return Result::fail(
                Core::Error::Facility::Graphics,
                Core::Error::Code::CreationFailed,
                Core::Error::Severity::Error,
                static_cast<uint32_t>(hr),
                "Failed to serialize root signature.");
        }
        ComPtr<ID3D12RootSignature> rootSignature;
        hr = m_renderDevice.get_d3d12_device()->CreateRootSignature(
            0,
            serializedRootSig->GetBufferPointer(),
            serializedRootSig->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature));
        if (FAILED(hr))
            {
            return Result::fail(
                Core::Error::Facility::Graphics,
                Core::Error::Code::CreationFailed,
                Core::Error::Severity::Error,
                static_cast<uint32_t>(hr),
                "Failed to create root signature.");
        }
        uint32_t newIndex = static_cast<uint32_t>(m_cache.size());
        m_cache.push_back(rootSignature);
        m_nameToCacheIndex[name.data()] = newIndex;
    
        return Result::ok();
    }
    Result RootSignatureCache::get(std::string_view name, ComPtr<ID3D12RootSignature>& outRootSignature)
    {
        std::lock_guard lock(m_mutex);
        if (m_nameToCacheIndex.contains(name.data()))
        {
            uint32_t index = m_nameToCacheIndex[name.data()];
            outRootSignature = m_cache[index];
            return Result::ok();
        }
        else
        {
            return Result::fail(
                Core::Error::Facility::Graphics,
                Core::Error::Code::NotFound,
                Core::Error::Severity::Error,
                0,
                "RootSignature not found in cache.");
        }
    }
    void RootSignatureCache::clear()
    {
        std::lock_guard lock(m_mutex);
        m_cache.clear();
        m_nameToCacheIndex.clear();
    }
    // RootSignatureCache の実装はここに記述されます
}

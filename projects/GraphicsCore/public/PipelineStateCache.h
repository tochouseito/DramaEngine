#pragma once
#include "stdafx.h"

// C++ standard includes
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Drama::Graphics::DX12
{
    class RenderDevice;
    class RootSignatureCache;
    class ShaderCompiler;

    enum class InputElementFormat : uint8_t
    {
        R32G32B32A32_Float,
        R32G32B32_Float,
        R32G32_Float,
        R32_Float,
    };

    struct InputElementDesc final
    {
        std::string semanticName = {};
        uint32_t semanticIndex = 0;
        InputElementFormat format = InputElementFormat::R32G32B32A32_Float;
    };

    enum class BlendMode : uint8_t
    {
        None,
        Normal
    };

    enum class CullMode : uint8_t
    {
        None,
        Front,
        Back
    };

    enum class FillMode : uint8_t
    {
        Solid,
        Wireframe
    };

    struct RasterizerStateDesc final
    {
        CullMode cullMode = CullMode::Back;
        FillMode fillMode = FillMode::Solid;
    };

    enum class DepthWriteMask : uint8_t
    {
        Zero,
        All
    };

    enum class ComparisonFunc : uint8_t
    {
        LessEqual,
    };

    enum class DSVFormat : uint8_t
    {
        D24_UNorm_S8_UInt,
    };

    struct DepthStencilStateDesc final
    {
        bool depthEnable = true;
        DepthWriteMask depthWriteMask = DepthWriteMask::All;
        ComparisonFunc depthFunc = ComparisonFunc::LessEqual;
    };

    enum class PrimitiveTopologyType : uint8_t
    {
        Triangle,
    };

    enum class RTVFormat : uint8_t
    {
        R8G8B8A8_UNorm,
        R8G8B8A8_UNorm_SRGB,
    };

    struct GraphicsPipelineStateDesc final
    {
        std::string rootSignatureName = {};
        std::string vertexShaderName = {};
        std::string pixelShaderName = {};
        std::vector<InputElementDesc> inputElements = {};
        std::vector<BlendMode> blendMode = { BlendMode::None };
        RasterizerStateDesc rasterizerState = {};
        DepthStencilStateDesc depthStencilState = {};
        DSVFormat dsvFormat = DSVFormat::D24_UNorm_S8_UInt;
        PrimitiveTopologyType primitiveTopologyType = PrimitiveTopologyType::Triangle;
        std::vector<RTVFormat> rtvFormats = {};
    };

    class PipelineStateCache final
    {
    public:
        /// @brief コンストラクタ
        explicit PipelineStateCache(RenderDevice& renderDevice, RootSignatureCache& rootSignatureCache, ShaderCompiler& shaderCompiler)
            : m_renderDevice(renderDevice)
            , m_rootSignatureCache(rootSignatureCache)
            , m_shaderCompiler(shaderCompiler)
        {
            // 1) 参照を保持するだけなので特別な初期化はしない
        }
        /// @brief デストラクタ
        ~PipelineStateCache() = default;

        /// @brief Graphics PSO を生成しキャッシュする
        [[nodiscard]] Result create_graphics(const std::string& name, const GraphicsPipelineStateDesc& desc);
        /// @brief Graphics PSO を取得する
        [[nodiscard]] Result get_graphics(const std::string& name, ComPtr<ID3D12PipelineState>& outPipelineState);

        /// @brief キャッシュをクリアする
        void clear();
    private:
        RenderDevice& m_renderDevice;
        RootSignatureCache& m_rootSignatureCache;
        ShaderCompiler& m_shaderCompiler;
        std::vector<ComPtr<ID3D12PipelineState>> m_graphicsCache;
        std::unordered_map<std::string, uint32_t> m_nameToGraphicsCacheIndex;
        std::vector<ComPtr<ID3D12PipelineState>> m_computeCache;
        std::unordered_map<std::string, uint32_t> m_nameToComputeCacheIndex;
        std::mutex m_mutex;
    };
}

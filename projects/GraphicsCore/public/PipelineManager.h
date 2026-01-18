#pragma once
#include "stdafx.h"
#include "ShaderCompiler.h"
#include <array>
#include <vector>
#include <unordered_map>

namespace Drama::Graphics::DX12
{
    // 前方宣言
    class RenderDevice;
    class ShaderCompiler;

    enum class BlendMode : uint8_t
    {
        None,
        Normal,
        Add,
        Subtract,
        Multiply,
        Screen,
        kCount ///< BlendModeの数
    };

    struct GraphicsPipelineDesc
    {
        std::string name = "";///< Pipeline name
        // Shader Desc
        ShaderCompileDesc vsDesc;///< Vertex Shader Compile Desc
        ShaderCompileDesc psDesc;///< Pixel Shader Compile Desc
        ShaderCompileDesc gsDesc;///< Geometry Shader Compile Desc
        ShaderCompileDesc hsDesc;///< Hull Shader Compile Desc
        ShaderCompileDesc dsDesc;///< Domain Shader Compile Desc
    };

    struct GraphicsPSO
    {
        ComPtr<ID3D12RootSignature> rootSignature = nullptr;
        std::array<ComPtr<ID3D12PipelineState>, static_cast<size_t>(BlendMode::kCount)> pso;
    };

    class PipelineManager final
    {
    public:
        /// @brief コンストラクタ
        PipelineManager(RenderDevice& renderDevice, ShaderCompiler& shaderCompiler)
            : m_renderDevice(renderDevice)
            , m_shaderCompiler(shaderCompiler)
        {
        }
        /// @brief デストラクタ
        ~PipelineManager() = default;

        void create_default_pipelines();
    private:
        void create_graphics_pipeline(GraphicsPipelineDesc& settings);
    private:
        RenderDevice& m_renderDevice;
        ShaderCompiler& m_shaderCompiler;

        std::vector<GraphicsPSO> m_graphicsPSOs;
        std::unordered_map<std::string, size_t> m_graphicsPipelineNameToIndex;
    };
} // namespace Drama::Graphics::DX12

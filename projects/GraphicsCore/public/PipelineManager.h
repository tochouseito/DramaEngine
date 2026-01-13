#pragma once
#include "stdafx.h"
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

    struct GraphicsPipelineSettings
    {
        std::string name = "";///< Pipeline name
        // D3D12 Objects
        ComPtr<ID3D12RootSignature> rootSignature = nullptr;
        std::array<ComPtr<ID3D12PipelineState>, static_cast<size_t>(BlendMode::kCount)> pso;
        // Shader names
        std::string vs = "";///< Vertex Shader
        std::string ps = "";///< Pixel Shader
        std::string gs = "";///< Geometry Shader
        std::string hs = "";///< Hull Shader
        std::string ds = "";///< Domain Shader
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
        void create_graphics_pipeline(GraphicsPipelineSettings& settings);
    private:
        RenderDevice& m_renderDevice;
        ShaderCompiler& m_shaderCompiler;

        std::vector<GraphicsPipelineSettings> m_graphicsPipelineSettings;
        std::unordered_map<std::string, size_t> m_graphicsPipelineNameToIndex;
    };
} // namespace Drama::Graphics::DX12

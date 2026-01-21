#pragma once
#include <cstdint>

namespace Drama::Graphics
{
    // 前方宣言
    namespace DX12
    {
        class RenderDevice;
        class DescriptorAllocator;
        class SwapChain;
        class CommandPool;
        class ShaderCompiler;
        class RootSignatureCache;
        class PipelineStateCache;
    }

    class GpuPipeline final
    {
    public:
        GpuPipeline(
            DX12::RenderDevice& renderDevice,
            DX12::DescriptorAllocator& descriptorAllocator,
            DX12::SwapChain& swapChain,
            DX12::CommandPool& commandPool,
            DX12::ShaderCompiler& shaderCompiler,
            DX12::RootSignatureCache& rootSignatureCache,
            DX12::PipelineStateCache& pipelineStateCache);
        ~GpuPipeline() = default;

        void render(uint64_t frameNo, uint32_t index);
        void present(uint64_t frameNo, uint32_t index);
    private:
        DX12::RenderDevice& m_renderDevice;
        DX12::DescriptorAllocator& m_descriptorAllocator;
        DX12::SwapChain& m_swapChain;
        DX12::CommandPool& m_commandPool;
        DX12::ShaderCompiler& m_shaderCompiler;
        DX12::RootSignatureCache& m_rootSignatureCache;
        DX12::PipelineStateCache& m_pipelineStateCache;
    };
}

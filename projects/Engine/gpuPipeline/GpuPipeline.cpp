#include "pch.h"
#include "GpuPipeline.h"

namespace Drama::Graphics
{
    GpuPipeline::GpuPipeline(DX12::RenderDevice& renderDevice, DX12::DescriptorAllocator& descriptorAllocator, DX12::SwapChain& swapChain, DX12::CommandPool& commandPool, DX12::ShaderCompiler& shaderCompiler, DX12::RootSignatureCache& rootSignatureCache, DX12::PipelineStateCache& pipelineStateCache)
        : m_renderDevice(renderDevice)
        , m_descriptorAllocator(descriptorAllocator)
        , m_swapChain(swapChain)
        , m_commandPool(commandPool)
        , m_shaderCompiler(shaderCompiler)
        , m_rootSignatureCache(rootSignatureCache)
        , m_pipelineStateCache(pipelineStateCache)
    {

    }
    void GpuPipeline::render(uint64_t frameNo, uint32_t index)
    {
    }
    void GpuPipeline::present(uint64_t frameNo, uint32_t index)
    {
    }
}

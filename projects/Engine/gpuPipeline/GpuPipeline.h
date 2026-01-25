#pragma once
#include <cstdint>
#include <memory>
#include <vector>

#include "Engine/gpuPipeline/GpuPipelineConfig.h"
#include "Engine/gpuPipeline/FrameGraph.h"

struct ID3D12Fence;

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

    class WorldResource;
    class ViewResource;
    class TransformWorldResource;

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
            DX12::PipelineStateCache& pipelineStateCache,
            GpuPipelineDesc desc = {});
        ~GpuPipeline();

        void register_pass(std::unique_ptr<FrameGraphPass> pass);
        void clear_registered_passes();

        void render(uint64_t frameNo, uint32_t index);
        void present(uint64_t frameNo, uint32_t index);
    private:
        void wait_for_frame(uint32_t index);

        DX12::RenderDevice& m_renderDevice;
        DX12::DescriptorAllocator& m_descriptorAllocator;
        DX12::SwapChain& m_swapChain;
        DX12::CommandPool& m_commandPool;
        DX12::ShaderCompiler& m_shaderCompiler;
        DX12::RootSignatureCache& m_rootSignatureCache;
        DX12::PipelineStateCache& m_pipelineStateCache;

        FrameGraph m_frameGraph;
        GpuPipelineDesc m_desc{};
        std::unique_ptr<FrameGraphPass> m_defaultPass;
        std::vector<std::unique_ptr<FrameGraphPass>> m_registeredPasses;

        std::vector<uint64_t> m_frameFenceValues;
        ID3D12Fence* m_graphicsFence = nullptr;

        std::vector<WorldResource*> m_worldResources;
        std::vector<ViewResource*> m_viewResources;
        std::unique_ptr<TransformWorldResource> m_transformWorldResource;
    };
}

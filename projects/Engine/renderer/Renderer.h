#pragma once
#include "GraphicsCore/public/GpuCommand.h"

namespace Drama::Graphics
{
    // 前方宣言
    namespace DX12
    {
        class RenderDevice;
        class DescriptorAllocator;
        class SwapChain;
    }

    class Renderer final
    {
    public:
        Renderer(DX12::RenderDevice& device, DX12::DescriptorAllocator& descriptorAllocator, DX12::SwapChain& swapChain);
        ~Renderer() = default;

        void render(uint64_t frameNo, uint32_t index);
        void present(uint64_t frameNo, uint32_t index);
    private:
        DX12::RenderDevice& m_renderDevice;
        DX12::DescriptorAllocator& m_descriptorAllocator;
        DX12::SwapChain& m_swapChain;

        std::unique_ptr<DX12::CommandPool> m_commandPool = nullptr;
    };
} // namespace Drama::Graphics::DX12


#pragma once
#include "stdafx.h"
#include "GpuCommand.h"

namespace Drama::Graphics::DX12
{
    // 前方宣言
    class RenderDevice;
    class DescriptorAllocator;
    class SwapChain;

    class Renderer final
    {
    public:
        Renderer(RenderDevice& device, DescriptorAllocator& descriptorAllocator, SwapChain& swapChain);
        ~Renderer() = default;

        void render(uint64_t frameNo, uint32_t index);
        void present(uint64_t frameNo, uint32_t index);
    private:
        RenderDevice& m_renderDevice;
        DescriptorAllocator& m_descriptorAllocator;
        SwapChain& m_swapChain;

        std::unique_ptr<CommandPool> m_commandPool = nullptr;
    };
} // namespace Drama::Graphics::DX12


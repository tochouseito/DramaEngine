#pragma once
#include "stdafx.h"
#include "GpuCommand.h"

namespace Drama::Graphics::DX12
{
    // 前方宣言
    class RenderDevice;

    class Renderer final
    {
    public:
        Renderer(RenderDevice& device);
        ~Renderer() = default;

        void Render(uint64_t frameNo, uint32_t index);
    private:
        RenderDevice& m_renderDevice;

        std::unique_ptr<CommandPool> m_commandPool = nullptr;
    };
} // namespace Drama::Graphics::DX12


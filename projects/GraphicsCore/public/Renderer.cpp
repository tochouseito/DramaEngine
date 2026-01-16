#include "pch.h"
#include "Renderer.h"
#include "RenderDevice.h"

namespace Drama::Graphics::DX12
{
    Renderer::Renderer(RenderDevice& device)
        : m_renderDevice(device)
    {
        m_commandPool = std::make_unique<CommandPool>(m_renderDevice);
    }
    void Renderer::Render(uint64_t frameNo, uint32_t index)
    {
        frameNo; index;
    }
} // namespace Drama::Graphics::DX12

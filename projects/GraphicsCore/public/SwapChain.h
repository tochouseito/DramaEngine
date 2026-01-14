#pragma once
#include "stdafx.h"
#include "DescriptorAllocator.h"
#include "Platform/windows/public/WindowsNative.h"
#include <vector>

namespace Drama::Graphics::DX12
{
    // 前方宣言
    class RenderDevice;
    class DescriptorAllocator;

    class SwapChain final
    {
        public:
            SwapChain(RenderDevice& device, DescriptorAllocator& descriptorAllocator, HWND& hWnd)
            : m_renderDevice(device)
            , m_descriptorAllocator(descriptorAllocator)
            , m_hWnd(hWnd)
        {
        }
        ~SwapChain() = default;

        [[nodiscard]] Core::Error::Result create(
            uint32_t width,
            uint32_t height,
            uint32_t bufferCount = 2);
    private:
        RenderDevice& m_renderDevice;
        DescriptorAllocator& m_descriptorAllocator;
        HWND& m_hWnd;

        ComPtr<IDXGISwapChain4> m_swapChain;
        DXGI_SWAP_CHAIN_DESC1 m_desc{};
        int32_t m_refreshrate = 60;
        std::vector<DescriptorAllocator::TableID> m_rtvTables;
    };
}

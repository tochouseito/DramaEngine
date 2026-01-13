#pragma once
#include "stdafx.h"
#include "GraphicsCore/public/GpuBuffer.h"

namespace Drama::Graphics::DX12
{
    // 前方宣言
    class RenderDevice;
    class DescriptorAllocator;

    class ResourceManager final
    {
    public:
        /// @brief コンストラクタ
        ResourceManager(RenderDevice& renderDevice, DescriptorAllocator& descriptorAllocator)
            : m_renderDevice(renderDevice), m_descriptorAllocator(descriptorAllocator)
        {
        }
        /// @brief デストラクタ
        ~ResourceManager() = default;


    private:
        RenderDevice& m_renderDevice;
        DescriptorAllocator& m_descriptorAllocator;
    };
} // namespace Drama::Graphics::DX12

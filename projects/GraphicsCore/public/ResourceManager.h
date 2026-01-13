#pragma once
#include "stdafx.h"
#include "GraphicsCore/public/RenderDevice.h"
#include "GraphicsCore/public/GpuBuffer.h"
#include <vector>
#include <mutex>

namespace Drama::Graphics::DX12
{
    // 前方宣言
    class RenderDevice;
    class DescriptorAllocator;

    class ResourceManager final
    {
    public:
        /// @brief コンストラクタ
        ResourceManager(RenderDevice& rd, DescriptorAllocator& da, uint32_t defaultBufferCap = 1024)
            : m_renderDevice(rd), m_descriptorAllocator(da)
        {
            m_gpuBuffers.reserve(defaultBufferCap);
        }
        /// @brief デストラクタ
        ~ResourceManager() = default;

        void destroy_gpu_buffer(uint32_t index)
        {
            std::lock_guard lock(m_gpuBufferMutex);
            if (index < m_gpuBuffers.size() && m_gpuBuffers[index])
            {
                m_gpuBuffers[index]->destroy();
                m_gpuBuffers[index].reset();
                release_gpu_buffer_index(index);
            }
        }

        template<typename T>
        [[nodiscard]] uint32_t create_constant_buffer()
        {
            std::lock_guard lock(m_gpuBufferMutex);
            uint32_t index = allocate_gpu_buffer_index();
            auto buffer = std::make_unique<ConstantBuffer<T>>();
            buffer->create(*m_renderDevice.get_d3d12_device(), L"ConstantBuffer");
            m_gpuBuffers[index] = std::move(buffer);
            return index;
        }

        template<typename T>
        [[nodiscard]] uint32_t create_structured_buffer(UINT numElements)
        {
            std::lock_guard lock(m_gpuBufferMutex);
            uint32_t index = allocate_gpu_buffer_index();
            auto buffer = std::make_unique<StructuredBuffer<T>>();
            buffer->create(*m_renderDevice.get_d3d12_device(), numElements, L"StructuredBuffer");
            m_gpuBuffers[index] = std::move(buffer);
            return index;
        }
    private:
        [[nodiscard]] uint32_t allocate_gpu_buffer_index()
        {
            if (!m_freeGpuBufferIndices.empty())
            {
                uint32_t index = m_freeGpuBufferIndices.back();
                m_freeGpuBufferIndices.pop_back();
                return index;
            }
            else
            {
                uint32_t newIndex = static_cast<uint32_t>(m_gpuBuffers.size());
                m_gpuBuffers.emplace_back(nullptr);
                return newIndex;
            }
        }
        void release_gpu_buffer_index(uint32_t index)
        {
            m_freeGpuBufferIndices.push_back(index);
        }
    private:
        RenderDevice& m_renderDevice;
        DescriptorAllocator& m_descriptorAllocator;

        std::vector<std::unique_ptr<GpuBuffer>> m_gpuBuffers;
        std::vector<uint32_t> m_freeGpuBufferIndices;
        std::mutex m_gpuBufferMutex;
    };
} // namespace Drama::Graphics::DX12

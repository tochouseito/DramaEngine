#pragma once
#include "stdafx.h"
#include "GraphicsCore/public/RenderDevice.h"
#include "GraphicsCore/public/GpuBuffer.h"
#include "GraphicsCore/public/DescriptorAllocator.h"
#include <vector>
#include <mutex>
#include <string_view>

namespace Drama::Graphics::DX12
{
    // 前方宣言
    class RenderDevice;
    class ResourceManager final
    {
    public:
        static constexpr uint32_t kInvalidBufferIndex = 0xFFFFFFFF;

        /// @brief コンストラクタ
        ResourceManager(RenderDevice& rd, DescriptorAllocator& da, uint32_t defaultBufferCap = 1024)
            : m_renderDevice(rd), m_descriptorAllocator(da)
        {
            m_gpuBuffers.reserve(defaultBufferCap);
        }
        /// @brief デストラクタ
        ~ResourceManager()
        {
            // 1) 残っているバッファとテーブルを解放する
            // 2) 管理配列を初期化して状態を戻す
            std::lock_guard lock(m_gpuBufferMutex);
            for (auto& entry : m_gpuBuffers)
            {
                if (entry.m_srvTable.valid())
                {
                    m_descriptorAllocator.free_table(entry.m_srvTable);
                    entry.m_srvTable = {};
                }
                if (entry.m_buffer)
                {
                    entry.m_buffer->destroy();
                    entry.m_buffer.reset();
                }
            }
            m_gpuBuffers.clear();
            m_freeGpuBufferIndices.clear();
        }

        void destroy_gpu_buffer(uint32_t index)
        {
            // 1) 対象バッファとテーブルを解放する
            // 2) 再利用できるようインデックスを返す
            std::lock_guard lock(m_gpuBufferMutex);
            if (index < m_gpuBuffers.size() && m_gpuBuffers[index].m_buffer)
            {
                if (m_gpuBuffers[index].m_srvTable.valid())
                {
                    m_descriptorAllocator.free_table(m_gpuBuffers[index].m_srvTable);
                    m_gpuBuffers[index].m_srvTable = {};
                }
                m_gpuBuffers[index].m_buffer->destroy();
                m_gpuBuffers[index].m_buffer.reset();
                release_gpu_buffer_index(index);
            }
        }

        template<typename T>
        [[nodiscard]] uint32_t create_constant_buffer()
        {
            return create_constant_buffer<T>(L"ConstantBuffer");
        }

        template<typename T>
        [[nodiscard]] uint32_t create_constant_buffer(std::wstring_view name)
        {
            // 1) バッファ枠を確保して定数バッファを作成する
            // 2) 生成したバッファを管理配列に保持する
            std::lock_guard lock(m_gpuBufferMutex);
            uint32_t index = allocate_gpu_buffer_index();
            auto buffer = std::make_unique<ConstantBuffer<T>>();
            buffer->create(*m_renderDevice.get_d3d12_device(), name);
            m_gpuBuffers[index].m_buffer = std::move(buffer);
            return index;
        }

        template<typename T>
        [[nodiscard]] uint32_t create_structured_buffer(UINT numElements)
        {
            return create_structured_buffer<T>(numElements, L"StructuredBuffer");
        }

        template<typename T>
        [[nodiscard]] uint32_t create_structured_buffer(UINT numElements, std::wstring_view name)
        {
            // 1) バッファ枠を確保して StructuredBuffer を作成する
            // 2) 生成したバッファを管理配列に保持する
            std::lock_guard lock(m_gpuBufferMutex);
            uint32_t index = allocate_gpu_buffer_index();
            auto buffer = std::make_unique<StructuredBuffer<T>>();
            buffer->create(*m_renderDevice.get_d3d12_device(), numElements, name);
            m_gpuBuffers[index].m_buffer = std::move(buffer);
            return index;
        }

        template<typename T>
        [[nodiscard]] uint32_t create_upload_buffer(UINT numElements, std::wstring_view name)
        {
            // 1) バッファ枠を確保して UploadBuffer を作成する
            // 2) 生成したバッファを管理配列に保持する
            std::lock_guard lock(m_gpuBufferMutex);
            uint32_t index = allocate_gpu_buffer_index();
            auto buffer = std::make_unique<UploadBuffer<T>>();
            buffer->create(*m_renderDevice.get_d3d12_device(), numElements, name);
            m_gpuBuffers[index].m_buffer = std::move(buffer);
            return index;
        }

        [[nodis]]

        [[nodiscard]] DescriptorAllocator::TableID create_srv_table(uint32_t index)
        {
            // 1) 対象バッファの存在を確認する
            // 2) SRV テーブルを作成して保持する
            std::lock_guard lock(m_gpuBufferMutex);
            if (index >= m_gpuBuffers.size())
            {
                return DescriptorAllocator::TableID{};
            }
            auto& entry = m_gpuBuffers[index];
            if (!entry.m_buffer)
            {
                return DescriptorAllocator::TableID{};
            }
            if (entry.m_srvTable.valid())
            {
                return entry.m_srvTable;
            }
            DescriptorAllocator::TableID table = m_descriptorAllocator.allocate(DescriptorAllocator::TableKind::Buffers);
            m_descriptorAllocator.create_srv_buffer(table, entry.m_buffer.get());
            entry.m_srvTable = table;
            return table;
        }

        [[nodiscard]] DescriptorAllocator::TableID get_srv_table(uint32_t index)
        {
            // 1) 範囲外なら無効値を返す
            // 2) 対応する SRV テーブルを返す
            std::lock_guard lock(m_gpuBufferMutex);
            if (index >= m_gpuBuffers.size())
            {
                return DescriptorAllocator::TableID{};
            }
            return m_gpuBuffers[index].m_srvTable;
        }

        [[nodiscard]] GpuBuffer* get_gpu_buffer(uint32_t index)
        {
            // 1) 範囲外や未生成は無効として扱う
            // 2) 非所有参照として返す
            std::lock_guard lock(m_gpuBufferMutex);
            if (index >= m_gpuBuffers.size())
            {
                return nullptr;
            }
            return m_gpuBuffers[index].m_buffer.get();
        }

        template<typename TBuffer>
        [[nodiscard]] TBuffer* get_gpu_buffer(uint32_t index)
        {
            // 1) 範囲外や未生成は無効として扱う
            // 2) 型一致時のみキャストして返す
            std::lock_guard lock(m_gpuBufferMutex);
            if (index >= m_gpuBuffers.size())
            {
                return nullptr;
            }
            auto* buffer = m_gpuBuffers[index].m_buffer.get();
            if (buffer == nullptr)
            {
                return nullptr;
            }
            if (buffer->get_buffer_type() != std::type_index(typeid(TBuffer)))
            {
                return nullptr;
            }
            return static_cast<TBuffer*>(buffer);
        }
    private:
        struct GpuBufferEntry final
        {
            std::unique_ptr<GpuBuffer> m_buffer = nullptr;
            DescriptorAllocator::TableID m_srvTable{};
        };

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
                m_gpuBuffers.emplace_back();
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

        std::vector<GpuBufferEntry> m_gpuBuffers;
        std::vector<uint32_t> m_freeGpuBufferIndices;
        std::mutex m_gpuBufferMutex;
    };
} // namespace Drama::Graphics::DX12

#pragma once

// === C++ Standard Library ===
#include <cstdint>
#include <memory>
#include <vector>

// === Engine ===
#include "Core/Error/Result.h"
#include "Engine/gpuPipeline/WorldResource.h"
#include "GraphicsCore/public/GpuBuffer.h"
#include "GraphicsCore/public/DescriptorAllocator.h"

namespace Drama::Graphics
{
    struct TransformData final
    {
        float m_matrix[16]{};
    };

    class TransformWorldResource final : public WorldResource
    {
    public:
        TransformWorldResource() = default;
        ~TransformWorldResource() override;

        Core::Error::Result initialize(
            DX12::RenderDevice& renderDevice,
            DX12::DescriptorAllocator& descriptorAllocator,
            uint32_t framesInFlight) override;

        void destroy() override;

        void add_passes(FrameGraph& frameGraph) override;

        DX12::DescriptorAllocator::TableID get_srv_table(uint32_t frameIndex) const;
        uint32_t get_capacity() const { return m_capacity; }

    private:
        class CopyPass final : public FrameGraphPass
        {
        public:
            CopyPass(TransformWorldResource& owner)
                : m_owner(owner)
            {
                // 1) 所有者参照を保持する
            }

            const char* get_name() const override
            {
                // 1) PIX 表示用の名前を返す
                return "TransformCopy";
            }

            PassType get_pass_type() const override
            {
                // 1) Copy パスとして扱う
                return PassType::Copy;
            }

            bool allow_copy_queue() const override
            {
                // 1) CopyQueue が有効なら移せるよう許可する
                return true;
            }

            void setup(FrameGraphBuilder& builder) override;
            void execute(FrameGraphContext& context) override;

        private:
            TransformWorldResource& m_owner;
            ResourceHandle m_src{};
            ResourceHandle m_dst{};
        };

    private:
        Core::Error::Result create_buffers(uint32_t framesInFlight);

    private:
        DX12::RenderDevice* m_renderDevice = nullptr;
        DX12::DescriptorAllocator* m_descriptorAllocator = nullptr;
        uint32_t m_framesInFlight = 1;
        uint32_t m_capacity = 0;
        uint64_t m_copyBytes = 0;

        std::vector<std::unique_ptr<DX12::UploadBuffer<TransformData>>> m_uploadBuffers;
        std::vector<std::unique_ptr<DX12::StructuredBuffer<TransformData>>> m_defaultBuffers;
        std::vector<DX12::DescriptorAllocator::TableID> m_srvTables;
        std::unique_ptr<CopyPass> m_copyPass;
    };
}

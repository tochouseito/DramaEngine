#pragma once

// === C++ Standard Library ===
#include <cstdint>
#include <memory>
#include <vector>

// === Engine ===
#include "Core/Error/Result.h"
#include "Math/public/Matrix4.h"
#include "Engine/gpuPipeline/worldResources/WorldResource.h"
#include "Engine/gpuPipeline/GpuPipelineConfig.h"
#include "GraphicsCore/public/DescriptorAllocator.h"

namespace Drama::Graphics
{
    namespace DX12
    {
        class ResourceManager;
    }

    struct ObjectData final
    {
        uint32_t id;
        uint32_t visible;
        uint32_t model_id;
        uint32_t transform_id;
    };
    
    class ObjectWorldResource final : public WorldResource
    {
    public:
        ObjectWorldResource() = default;
        ~ObjectWorldResource() override;

        void set_transform_buffer_mode(TransformBufferMode mode)
        {
            // 1) モードを保持して初期化に反映する
            m_transformBufferMode = mode;
        }
        void set_capacity(uint32_t capacity)
        {
            // 1) 0 を許容しないため最低値を保証する
            m_capacity = (capacity == 0) ? 1 : capacity;
        }
        Core::Error::Result initialize(
            DX12::ResourceManager& resourceManager,
            uint32_t framesInFlight) override;

        void destroy() override;

        void add_passes(FrameGraph& frameGraph) override;

        DX12::DescriptorAllocator::TableID get_srv_table(uint32_t frameIndex) const;
        uint32_t get_capacity() const { return m_capacity; }
    private:
        class CopyPass final : public FrameGraphPass
        {
        public:
            CopyPass(ObjectWorldResource& owner)
                : m_owner(owner)
            {
                // 1) 所有者参照を保持する
            }

            const char* get_name() const override
            {
                // 1) PIX 表示用の名前を返す
                return "ObjectCopy";
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
            ObjectWorldResource& m_owner;
            ResourceHandle m_src{};
            ResourceHandle m_dst{};
        };
    private:
        Core::Error::Result create_buffers(uint32_t framesInFlight);
    private:
        DX12::ResourceManager* m_resourceManager = nullptr;
        uint32_t m_framesInFlight = 0;
        uint32_t m_capacity = 0;
        uint64_t m_copyBytes = 0;
        TransformBufferMode m_transformBufferMode = TransformBufferMode::DefaultWithStaging;

        std::vector<uint32_t> m_uploadBufferIds;
        std::vector<uint32_t> m_defaultBufferIds;
        std::vector<DX12::DescriptorAllocator::TableID> m_srvTables;
        std::unique_ptr<CopyPass> m_copyPass;
    };
}



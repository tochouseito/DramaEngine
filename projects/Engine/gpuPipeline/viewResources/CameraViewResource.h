#pragma once

// === C++ Standard Library ===
#include <cstdint>
#include <memory>
#include <vector>

// === Engine ===
#include "Core/Error/Result.h"
#include "Math/public/Matrix4.h"
#include "Engine/gpuPipeline/viewResources/ViewResource.h"
#include "Engine/gpuPipeline/GpuPipelineConfig.h"
#include "GraphicsCore/public/DescriptorAllocator.h"

namespace Drama::Graphics
{
    namespace DX12
    {
        class ResourceManager;
    }

    struct CameraData final
    {
        Math::float4x4 viewMatrix;
        Math::float4x4 projectionMatrix;
    };

    class CameraViewResource final : public ViewResource
    {
    public:
        CameraViewResource() = default;
        ~CameraViewResource() override;

        void set_transform_buffer_mode(TransformBufferMode mode)
        {
            // 1) モードを保持して初期化に反映する
            m_transformBufferMode = mode;
        }

        Core::Error::Result initialize(
            DX12::ResourceManager& resourceManager,
            uint32_t framesInFlight) override;

        void destroy() override;

        void add_passes(FrameGraph& frameGraph) override;

        DX12::DescriptorAllocator::TableID get_cbv_table(uint32_t frameIndex) const;
    private:
        class CopyPass final : public FrameGraphPass
        {
        public:
            CopyPass(CameraViewResource& owner)
                : m_owner(owner)
            {
                // 1) 所有者参照を保持する
            }

            const char* get_name() const override
            {
                // 1) PIX 表示用の名前を返す
                return "CameraCopy";
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

            void setup_static(FrameGraphBuilder& builder) override;
            void update_imports(FrameGraphBuilder& builder) override;
            void execute(FrameGraphContext& context) override;

        private:
            CameraViewResource& m_owner;
            ResourceHandle m_src{};
            ResourceHandle m_dst{};
        };
    private:
        Core::Error::Result create_buffers(uint32_t framesInFlight);
    private:
        DX12::ResourceManager* m_resourceManager = nullptr;
        uint32_t m_framesInFlight = 1;
        uint32_t m_copyBytes = 0;
        TransformBufferMode m_transformBufferMode = TransformBufferMode::DefaultWithStaging;

        std::vector<uint32_t> m_uploadBufferIds;
        std::vector<uint32_t> m_defaultBufferIds;
        std::vector<DX12::DescriptorAllocator::TableID> m_cbvTables;
        std::unique_ptr<CopyPass> m_copyPass;
    };
}

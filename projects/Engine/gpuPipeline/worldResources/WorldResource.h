#pragma once

// === C++ Standard Library ===
#include <cstdint>

// === Engine ===
#include "Core/Error/Result.h"
#include "Engine/gpuPipeline/FrameGraph.h"

namespace Drama::Graphics
{
    namespace DX12
    {
        class ResourceManager;
    }

    class WorldResource
    {
    public:
        virtual ~WorldResource() = default;

        virtual Core::Error::Result initialize(
            DX12::ResourceManager& resourceManager,
            uint32_t framesInFlight) = 0;

        virtual void destroy() = 0;

        virtual void set_frame_index(uint32_t frameIndex)
        {
            // 1) 現在フレームのリソース参照先を更新する
            m_frameIndex = frameIndex;
        }

        virtual void add_passes(FrameGraph& frameGraph) = 0;

    protected:
        uint32_t m_frameIndex = 0;
    };
}

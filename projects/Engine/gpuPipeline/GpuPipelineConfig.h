#pragma once
#include <cstdint>

namespace Drama::Graphics
{
    enum class RenderMode : uint8_t
    {
        Forward = 0,
        Deferred = 1,
        Hybrid = 2
    };

    enum class TransparencyMode : uint8_t
    {
        NormalBlend = 0,
        Oit = 1
    };

    enum class TransformBufferMode : uint8_t
    {
        UploadOnly = 0,
        DefaultWithStaging = 1
    };

    struct GpuPipelineDesc final
    {
        uint32_t m_framesInFlight = 2;
        RenderMode m_renderMode = RenderMode::Forward;
        TransparencyMode m_transparencyMode = TransparencyMode::NormalBlend;
        TransformBufferMode m_transformBufferMode = TransformBufferMode::DefaultWithStaging;
        uint32_t m_transformBufferCapacity = 1024;
        bool m_enableAsyncCompute = false;
        bool m_enableCopyQueue = false;
    };
}

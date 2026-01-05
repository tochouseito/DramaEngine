#pragma once
#include <cstdint>

namespace Drama::Core
{
    enum class HandleType : uint8_t
    {
        Unknown = 0,
        Texture = 1,
        Buffer = 2,
        Shader = 3,
        PipelineState = 4,
        RenderTargetView = 5,
        DepthStencilView = 6,
    };

    struct Handle final
    {
        uint32_t id = UINT32_MAX;// 無効なハンドルID

        void clear() noexcept
        {
            id = UINT32_MAX;
        }

        explicit operator bool() const noexcept
        {
            // 1) 成功判定
            return id != UINT32_MAX;
        }
    };
}

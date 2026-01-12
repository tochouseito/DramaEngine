#pragma once
#include "stdafx.h"

namespace Drama::Graphics
{
    struct GraphicsConfig
    {

        //======= DXGI Formats =======//
        DXGI_FORMAT m_ldrOffscreenFormat = DXGI_FORMAT_R8G8B8A8_UNORM; // LDRオフスクリーンのフォーマット
        DXGI_FORMAT m_hdrOffscreenFormat = DXGI_FORMAT_R16G16B16A16_FLOAT; // HDRオフスクリーンのフォーマット
        DXGI_FORMAT m_depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT; // 深度ステンシルのフォーマット

        //=======Debug Settings=======//
        bool m_enableDebugLayer = false; // デバッグレイヤーを有効化するか
    };

    extern GraphicsConfig graphicsConfig;
}

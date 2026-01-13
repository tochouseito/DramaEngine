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

        //======= Shader Settings =======//
        D3D_SHADER_MODEL m_highestShaderModel = D3D_SHADER_MODEL_NONE;  ///< 利用可能な最高シェーダーモデル
        D3D_SHADER_MODEL m_requestedShaderModel = D3D_SHADER_MODEL_6_5;  ///< 要求するシェーダーモデル

        //=======Debug Settings=======//
        bool m_enableDebugLayer = false; // デバッグレイヤーを有効化するか
    };

    extern GraphicsConfig graphicsConfig;
}

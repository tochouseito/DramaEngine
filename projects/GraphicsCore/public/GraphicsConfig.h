#pragma once
#include "stdafx.h"
#include <array>

namespace Drama::Graphics
{
    struct GraphicsConfig
    {
        //======== Display Settings =======//
        uint32_t m_screenWidth = 1280; // 画面の幅(px)
        uint32_t m_screenHeight = 720; // 画面の高さ(px)
        uint32_t m_resolutionWidth = 1280; // 解像度の幅(px)
        uint32_t m_resolutionHeight = 720; // 解像度の高さ(px)
        uint32_t m_displayRefreshrate = 60; // ディスプレイのリフレッシュレート(Hz)
        bool m_enableVSync = true; // VSyncを有効化するか
        std::array<float, 4> m_clearColor = { 0.1f,0.25f,0.5f,1.0f }; ///< クリアカラー

        uint32_t m_bufferingCount = 3; // バッファリング数

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

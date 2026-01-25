#pragma once

#include <cstdint>
#include <string>
#include <d3d12.h>
#include <dxgiformat.h>
#include "Engine/gpuPipeline/GpuPipelineConfig.h"

namespace Drama
{
    struct EngineConfig
    {
        uint32_t m_bufferingCount = 3; ///< バッファリング数

        //=======Debug Settings=======//
        bool m_enableDebugLayer = false; // デバッグレイヤーを有効化するか

        //======= Cache Paths =======//
        std::string m_shaderCacheDirectory = "cache/shaders/"; ///< シェーダーキャッシュディレクトリ

        //======= GpuPipeline Settings =======//
        Graphics::RenderMode m_renderMode = Graphics::RenderMode::Forward;
        Graphics::TransparencyMode m_transparencyMode = Graphics::TransparencyMode::NormalBlend;
        Graphics::TransformBufferMode m_transformBufferMode = Graphics::TransformBufferMode::DefaultWithStaging;
        uint32_t m_transformBufferCapacity = 1024;
        bool m_enableAsyncCompute = false;
        bool m_enableCopyQueue = false;
    };

    extern EngineConfig g_engineConfig;

    extern const std::string version;
    extern const std::string appName;

    namespace FilePath
    {
        extern const std::string engineConfigIniPath; ///< エンジン設定ファイルパス
        extern const std::string engineLogPath;    ///< エンジンログファイルパス

        extern const std::string shaderDirectory;       ///< シェーダーディレクトリ
        extern const std::string shaderCacheDirectory; ///< シェーダーキャッシュディレクトリ
        extern const std::string graphicsPipelinesIniPath;  ///< グラフィックスパイプライン設定ファイルパス
        extern const std::string computePipelinesIniPath;   ///< コンピュートパイプライン設定ファイルパス
        extern const std::string meshPipelinesIniPath;      ///< メッシュパイプライン設定ファイルパス

        extern const std::string imGuiIniPath;    ///< ImGui設定ファイルパス

        
    }

    namespace Graphics
    {
        /// @brief グラフィックス設定
        extern uint32_t resolutionWidth;    ///< 解像度幅
        extern uint32_t resolutionHeight;   ///< 解像度高さ

        constexpr uint32_t kMaxBufferingCount = 3; ///< 最大バッファリング数
        extern uint32_t bufferingCount; ///< バッファリング数
        extern uint32_t displayRefreshRate;          ///< 最大FPS(モニターのリフレッシュレート)

        extern const float kClearColor[4]; ///< クリアカラー
        extern DXGI_FORMAT defaultDxgiFormat;
        extern DXGI_FORMAT defaultDepthDxgiFormat;
        extern bool enableVSync;          ///< VSync有効化フラグ

        extern D3D_SHADER_MODEL highestShaderModel;  ///< 利用可能な最高シェーダーモデル
        extern D3D_SHADER_MODEL requestedShaderModel;  ///< 要求するシェーダーモデル
    }
}


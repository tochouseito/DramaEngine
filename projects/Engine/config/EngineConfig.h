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
        //=======Debug Settings=======//
        bool m_enableDebugLayer = false; // デバッグレイヤーを有効化するか

        //======= Cache Paths =======//
        std::string m_shaderCacheDirectory = "cache/shaders/"; ///< シェーダーキャッシュディレクトリ
        std::string m_imGuiIniPath = "config/editor/imgui.ini";    ///< ImGui設定ファイルパス
        std::string engineConfigIniPath = "config/engineConfig.ini"; ///< エンジン設定ファイルパス
        std::string engineLogPath = "temp/log/engine_log.txt";    ///< エンジンログファイルパス

        //======= GpuPipeline Settings =======//
        Graphics::RenderMode m_renderMode = Graphics::RenderMode::Forward;
        Graphics::TransparencyMode m_transparencyMode = Graphics::TransparencyMode::NormalBlend;
        Graphics::TransformBufferMode m_transformBufferMode = Graphics::TransformBufferMode::DefaultWithStaging;
        uint32_t m_transformBufferCapacity = 1024;
        bool m_enableAsyncCompute = false;
        bool m_enableCopyQueue = false;
    };

    extern EngineConfig g_engineConfig;
}


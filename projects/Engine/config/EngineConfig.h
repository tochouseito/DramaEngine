#pragma once

#include <cstdint>
#include <string>
#include <d3d12.h>
#include <dxgiformat.h>

namespace Drama::EngineConfig
{
    extern const std::string version;
    extern const std::string appName;

    namespace FilePath
    {
        extern const std::string ShaderDirectory;       ///< シェーダーディレクトリ
        extern const std::string ShaderCacheDirectory; ///< シェーダーキャッシュディレクトリ
        extern const std::string GraphicsPipelines_iniPath;  ///< グラフィックスパイプライン設定ファイルパス
        extern const std::string ComputePipelines_iniPath;   ///< コンピュートパイプライン設定ファイルパス
        extern const std::string MeshPipelines_iniPath;      ///< メッシュパイプライン設定ファイルパス

        extern const std::string ImGui_iniPath;    ///< ImGui設定ファイルパス

        extern const std::string EngineLogPath;    ///< エンジンログファイルパス
    }

    namespace Graphics
    {
        /// @brief グラフィックス設定
        extern uint32_t ResolutionWidth;    ///< 解像度幅
        extern uint32_t ResolutionHeight;   ///< 解像度高さ

        constexpr uint32_t kMaxBufferingCount = 3; ///< 最大バッファリング数
        extern uint32_t BufferingCount; ///< バッファリング数
        extern uint32_t DisplayRefreshrate;          ///< 最大FPS(モニターのリフレッシュレート)

        extern const float kClearColor[4]; ///< クリアカラー
        extern DXGI_FORMAT DefaultDXGIFormat;
        extern DXGI_FORMAT DefaultDepthDXGIFormat;
        extern bool EnableVSync;          ///< VSync有効化フラグ

        extern D3D_SHADER_MODEL HighestShaderModel;  ///< 利用可能な最高シェーダーモデル
        extern D3D_SHADER_MODEL RequestedShaderModel;  ///< 要求するシェーダーモデル
    }
}


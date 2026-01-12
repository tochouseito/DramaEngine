#include "pch.h"
#include "EngineConfig.h"

namespace Drama
{
    EngineConfig g_engineConfig = {};

    const std::string version = "0.0.1";
    const std::string appName = "Theatria Engine";

    namespace FilePath
    {
        const std::string engineConfigIniPath = "config/engineConfig.ini"; ///< エンジン設定ファイルパス
        const std::string engineLogPath = "temp/log/engine_log.txt";    ///< エンジンログファイルパス

        const std::string shaderDirectory = "shader/";       ///< シェーダーディレクトリ
        const std::string shaderCacheDirectory = "temp/cache/shader/"; ///< シェーダーキャッシュディレクトリ
        const std::string graphicsPipelinesIniPath = "config/pipelines/graphicsPipelines.ini";  ///< グラフィックスパイプライン設定ファイルパス
        const std::string computePipelinesIniPath = "config/pipelines/computePipelines.ini";   ///< コンピュートパイプライン設定ファイルパス
        const std::string meshPipelinesIniPath = "config/pipelines/meshPipelines.ini";      ///< メッシュパイプライン設定ファイルパス

        const std::string imGuiIniPath = "config/editor/imgui.ini";    ///< ImGui設定ファイルパス

        
    }

    namespace Graphics
    {
        uint32_t resolutionWidth = 1920;    ///< 解像度幅
        uint32_t resolutionHeight = 1080;   ///< 解像度高さ

        uint32_t bufferingCount = 3; ///< バッファリング数
        uint32_t displayRefreshRate = 60;          ///< 最大FPS(モニターのリフレッシュレート)

        const float kClearColor[4] = { 0.1f,0.25f,0.5f,1.0f }; ///< クリアカラー
        DXGI_FORMAT defaultDxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        DXGI_FORMAT defaultDepthDxgiFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        bool enableVSync = true;          ///< VSync有効化フラグ

        D3D_SHADER_MODEL highestShaderModel = D3D_SHADER_MODEL_6_8;  ///< 利用可能な最高シェーダーモデル
        D3D_SHADER_MODEL requestedShaderModel = D3D_SHADER_MODEL_6_0;  ///< 要求するシェーダーモデル
    }
}

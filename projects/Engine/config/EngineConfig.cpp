#include "pch.h"
#include "EngineConfig.h"

namespace Drama::EngineConfig
{
    const std::string version = "0.0.1";
    const std::string appName = "Theatria Engine";

    namespace FilePath
    {
        const std::string ShaderDirectory = "shader/";       ///< シェーダーディレクトリ
        const std::string ShaderCacheDirectory = "temp/cache/shader/"; ///< シェーダーキャッシュディレクトリ
        const std::string GraphicsPipelines_iniPath = "config/pipelines/graphicsPipelines.ini";  ///< グラフィックスパイプライン設定ファイルパス
        const std::string ComputePipelines_iniPath = "config/pipelines/computePipelines.ini";   ///< コンピュートパイプライン設定ファイルパス
        const std::string MeshPipelines_iniPath = "config/pipelines/meshPipelines.ini";      ///< メッシュパイプライン設定ファイルパス

        const std::string ImGui_iniPath = "config/editor/imgui.ini";    ///< ImGui設定ファイルパス
    }

    namespace Graphics
    {
        uint32_t ResolutionWidth = 1920;    ///< 解像度幅
        uint32_t ResolutionHeight = 1080;   ///< 解像度高さ

        uint32_t BufferingCount = 3; ///< バッファリング数
        uint32_t DisplayRefreshrate = 60;          ///< 最大FPS(モニターのリフレッシュレート)

        const float kClearColor[4] = { 0.1f,0.25f,0.5f,1.0f }; ///< クリアカラー
        DXGI_FORMAT DefaultDXGIFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        DXGI_FORMAT DefaultDepthDXGIFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        bool EnableVSync = true;          ///< VSync有効化フラグ

        D3D_SHADER_MODEL HighestShaderModel = D3D_SHADER_MODEL_6_8;  ///< 利用可能な最高シェーダーモデル
        D3D_SHADER_MODEL RequestedShaderModel = D3D_SHADER_MODEL_6_0;  ///< 要求するシェーダーモデル
    }
}

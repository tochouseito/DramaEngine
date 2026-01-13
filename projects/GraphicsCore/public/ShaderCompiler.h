#pragma once
#include "stdafx.h"

namespace Drama::Graphics::DX12
{
    // 前方宣言
    class RenderDevice;

    struct ShaderCompileDesc final
    {
        std::wstring filePath = {};               ///< シェーダーファイルのパス
        std::wstring entryPoint = {};            ///< エントリーポイント名
        std::wstring targetProfile = {};         ///< ターゲットプロファイル名
        bool enableDebugInfo = true;          ///< デバッグ情報を有効にするかどうか
    };

    std::wstring shader_profile_to_wstring(D3D_SHADER_MODEL model);

    class ShaderCompiler final
    {
    public:
                /// @brief コンストラクタ
        ShaderCompiler();
        /// @brief デストラクタ
        ~ShaderCompiler() = default;

        ComPtr<IDxcBlob> get_compile_shader(const ShaderCompileDesc& desc);
    private:
        ComPtr<IDxcBlob> compile_shader_raw(const ShaderCompileDesc& desc);
    private:
        ComPtr<IDxcUtils> m_dxcUtils = nullptr;
        ComPtr<IDxcCompiler3> m_dxcCompiler = nullptr;
        ComPtr<IDxcIncludeHandler> m_dxcIncludeHandler = nullptr;
    };
} // namespace Drama::Graphics::DX12


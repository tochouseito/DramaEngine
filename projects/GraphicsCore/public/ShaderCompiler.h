#pragma once
#include "stdafx.h"
#include <unordered_map>
#include <vector>

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

        explicit operator bool() const noexcept
        {
            if (filePath.empty() || entryPoint.empty() || targetProfile.empty())
            {
                return false;
            }
            return true;
        }
    };

    std::wstring shader_profile_to_wstring(D3D_SHADER_MODEL model);

    class ShaderCompiler final
    {
    public:
        /// @brief コンストラクタ
        ShaderCompiler(const std::string& cachePath);
        /// @brief デストラクタ
        ~ShaderCompiler() = default;

        [[nodiscard]] Result get_shader_blob(std::string_view name, ComPtr<IDxcBlob> outBlob);
    private:
        ComPtr<IDxcBlob> compile_shader_raw(const ShaderCompileDesc& desc);
    private:
        ComPtr<IDxcUtils> m_dxcUtils = nullptr;
        ComPtr<IDxcCompiler3> m_dxcCompiler = nullptr;
        ComPtr<IDxcIncludeHandler> m_dxcIncludeHandler = nullptr;
        std::string m_cachePath = {};
        std::vector<ComPtr<IDxcBlob>> m_cache;
        std::unordered_map<std::string, uint32_t> m_nameToCacheIndex;
    };
} // namespace Drama::Graphics::DX12


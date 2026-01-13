#pragma once
#include "stdafx.h"

namespace Drama::Graphics::DX12
{
    // 前方宣言
    class RenderDevice;

    class ShaderCompiler final
    {
    public:
                /// @brief コンストラクタ
        ShaderCompiler() = default;
        /// @brief デストラクタ
        ~ShaderCompiler() = default;

    private:
        ComPtr<IDxcUtils> m_dxcUtils = nullptr;
        ComPtr<IDxcCompiler3> m_dxcCompiler = nullptr;
        ComPtr<IDxcIncludeHandler> m_dxcIncludeHandler = nullptr;
    };
} // namespace Drama::Graphics::DX12


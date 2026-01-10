#pragma once
#include "stdafx.h"

namespace Drama::Graphics::DX12
{
    /// @brief リソースリークチェッカー
    class ResourceLeakChecker final
    {
    public:
        /// @brief デストラクタ
        ~ResourceLeakChecker();
    };
}

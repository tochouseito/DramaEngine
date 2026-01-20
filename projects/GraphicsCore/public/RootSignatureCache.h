#pragma once
#include "stdafx.h"

// C++ standard includes
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Drama::Graphics::DX12
{
    class RenderDevice;

    enum class RootParameterType : uint8_t
    {
        CBV,
        SRV
    };

    enum class ShaderVisibility : uint8_t
    {
        All,
        Vertex,
        Pixel
    };

    struct RootParameterDesc
    {
        RootParameterType type;
        ShaderVisibility visibility;
        uint32_t shaderRegister;
    };

    struct RootSignatureDesc
    {
        std::vector<RootParameterDesc> parameters;
    };

    class RootSignatureCache final
    {
    public:
        /// @brief コンストラクタ
        explicit RootSignatureCache(RenderDevice& renderDevice)
            : m_renderDevice(renderDevice)
        {
            // 1) 参照を保持するだけなので特別な初期化はしない
        }
        /// @brief デストラクタ
        ~RootSignatureCache() = default;

        /// @brief RootSignature を生成しキャッシュする
        [[nodiscard]] Result create(std::string_view name, const RootSignatureDesc& desc);

        [[nodiscard]] Result get(std::string_view name, ComPtr<ID3D12RootSignature>& outRootSignature);

        /// @brief キャッシュをクリアする
        void clear();
    private:
        RenderDevice& m_renderDevice;
        std::vector<ComPtr<ID3D12RootSignature>> m_cache;
        std::unordered_map<std::string, uint32_t> m_nameToCacheIndex;
        std::mutex m_mutex;
    };
}

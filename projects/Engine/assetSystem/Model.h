#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "Math/public/Vector2.h"
#include "Math/public/Vector3.h"
#include "Math/public/Vector4.h"

namespace Drama::Asset
{
    /// @brief 頂点データ構造体
    struct VertexData
    {
        Math::float4 position;   // 位置
        Math::float2 uv;         // UV座標
        Math::float3 normal;     // 法線
    };

    struct MeshData
    {
        std::string name;               // メッシュ名
        std::vector<VertexData> vertices; // 頂点データ配列
        std::vector<std::uint32_t> indices; // インデックスデータ配列
        static constexpr uint32_t k_invalidBufferIndex = 0xFFFFFFFF;
        uint32_t vertexBufferIndex = k_invalidBufferIndex; // 頂点バッファのインデックス
        uint32_t indexBufferIndex = k_invalidBufferIndex;  // インデックスバッファのインデックス
    };

    struct ModelData
    {
        std::vector<MeshData> meshes; // メッシュデータ配列
    };
}

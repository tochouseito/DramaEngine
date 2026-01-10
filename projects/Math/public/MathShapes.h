#pragma once
#include"Vector3.h"

namespace Drama::Math
{

    struct Sphere
    {
        float3 center; // !< 中心点
        float radius;   // !< 半径
    };
    struct Line
    {
        float3 origin; // !<始点
        float3 diff;   // !<終点への差分ベクトル
    };
    struct Ray
    {
        float3 origin; // !<始点
        float3 diff;   // !<終点への差分ベクトル
    };
    struct Segment
    {
        float3 origin; // !<始点
        float3 diff;   // !<終点への差分ベクトル
    };
    struct Plane
    {
        float3 normal; //!< 法線
        float distance; //!< 距離
    };
    struct Triangle
    {
        float3 vertices[3];//!< 頂点
    };
    struct AABB
    {
        float3 min; //!<最小点
        float3 max; //!<最大点
    };
    struct Vector2Int
    {
        int x;
        int y;
    };
    struct OBB
    {
        float3 center; //!<中心点
        float3 orientations[3]; //!<座標軸、正規化，直交必須
        float3 size; //!< 座標方向の長さの半分。中心から面までの距離
    };
}

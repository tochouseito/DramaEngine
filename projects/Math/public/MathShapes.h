#pragma once
#include "Vector3.h"

namespace Drama::Math
{
    struct Sphere final
    {
        Float3 m_center; // 中心点
        float m_radius; // 半径
    };

    struct Line final
    {
        Float3 m_origin; // 始点
        Float3 m_diff; // 終点への差分ベクトル
    };

    struct Ray final
    {
        Float3 m_origin; // 始点
        Float3 m_diff; // 終点への差分ベクトル
    };

    struct Segment final
    {
        Float3 m_origin; // 始点
        Float3 m_diff; // 終点への差分ベクトル
    };

    struct Plane final
    {
        Float3 m_normal; // 法線
        float m_distance; // 距離
    };

    struct Triangle final
    {
        Float3 m_vertices[3]; // 頂点
    };

    struct Aabb final
    {
        Float3 m_min; // 最小点
        Float3 m_max; // 最大点
    };

    struct Vector2Int final
    {
        int m_x;
        int m_y;
    };

    struct Obb final
    {
        Float3 m_center; // 中心点
        Float3 m_orientations[3]; // 座標軸、正規化・直交が必要
        Float3 m_size; // 座標方向の長さの半分。中心から面までの距離
    };
} // Drama::Math 名前空間

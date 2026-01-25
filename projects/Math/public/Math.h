#pragma once

/*
行ベクトル row-vector
行優先 row-major
*/

#include "Vector2.h"
#include "Vector3.h"
#include "Scale.h"
#include "Vector4.h"
#include "Quaternion.h"
#include "Matrix4.h"
#include "MathShapes.h"

#include <cstdint>
#include <numbers>

namespace Drama::Math
{
    constexpr float pi = std::numbers::pi_v<float>;

    enum class RotationOrder
    {
        Xyz,
        Yxz,
        Zxy,
        Zyx,
        Yzx,
        Xzy
    };

    // Srt をまとめた構造体
    struct Srt final
    {
        float3 m_translation;
        float3 m_rotationEuler; // ラジアン or 度数法 に合わせる
        Scale m_scale;
    };

    template <class T>
    [[nodiscard]] constexpr T clamp(T x, T minValue, T maxValue) noexcept
    {
        // 1) 範囲が逆なら入れ替えて安全化する
        if (maxValue < minValue)
        {
            T temp = minValue;
            minValue = maxValue;
            maxValue = temp;
        }
        // 2) 範囲内に収める
        return (x < minValue) ? minValue : (x > maxValue) ? maxValue : x;
    }

    /// @brief 回転行列→クォータニオン（スケール除去＆数値ガード＆正規化付き）
    /// 行列 m は row-major で m[row][col] としてアクセス可能な想定。
    /// 3x3 部分に回転以外（スケール等）が混ざっていても列正規化である程度補正します。
    Quaternion from_matrix(const float4x4& m) noexcept;

    [[nodiscard]] float3 transform_point(const float3& v, const float4x4& matrix) noexcept;

    [[nodiscard]] float3 transform_vector(const float3& v, const float4x4& matrix) noexcept;

    [[nodiscard]] float4x4 translate_matrix(const float3& translation) noexcept;

    [[nodiscard]] float4x4 scale_matrix(float scale) noexcept;

    [[nodiscard]] float4x4 scale_matrix(const float3& scale) noexcept;

    [[nodiscard]] float4x4 scale_matrix(const Scale& scale) noexcept;

    [[nodiscard]] float4x4 x_axis_matrix(float radian) noexcept;

    [[nodiscard]] float4x4 y_axis_matrix(float radian) noexcept;

    [[nodiscard]] float4x4 z_axis_matrix(float radian) noexcept;

    [[nodiscard]] float4x4 rotate_xyz_matrix(const float3& rotation) noexcept;

    [[nodiscard]] float4x4 viewport_matrix(float left, float top, float width, float height, float minDepth, float maxDepth) noexcept;

    [[nodiscard]] float4x4 perspective_fov_matrix(float fovY, float aspectRatio, float nearClip, float farClip) noexcept;

    [[nodiscard]] float4x4 orthographic_matrix(float left, float top, float right, float bottom, float nearClip, float farClip) noexcept;

    [[nodiscard]] float normalize(float value, float minValue, float maxValue) noexcept;

    /*=========== レガシー ===========*/

    float4x4 make_rotate_axis_angle(const float3& axis, float angle);

    float4x4 direction_to_direction(const float3& from, const float3& to);

    float degrees_to_radians(float degrees);

    float3 degrees_to_radians(const float3& degrees);

    float radians_to_degrees(float radians);

    float3 radians_to_degrees(const float3& radians);

    Quaternion make_rotate_axis_angle_quaternion(const float3& axis, float angle);

    float3 rotate_vector(const float3& vector, const Quaternion& quaternion);

    float4x4 make_rotate_matrix(const Quaternion& quaternion);

    Quaternion from_euler_angles(const float3& eulerAngles);

    float3 to_euler_angles(const Quaternion& q, RotationOrder order);

    float4x4 make_affine_matrix(const float3& scale, const float3& rotate, const float3& translate);

    float4x4 make_affine_matrix(const Scale& scale, const Quaternion& rotate, const float3& translate);

    float3 transform_direction(const float3& v, const float4x4& matrix);

    float lerp_short_angle(float startAngle, float endAngle, float t);

    Quaternion make_look_rotation(const float3& forward, const float3& up);

    float3 get_forward_vector_from_matrix(const float4x4& rotMatrix);

    Srt decompose_matrix(const float4x4& in);

    Quaternion make_quaternion_rotation(const float3& rad, const float3& preRad, const Quaternion& quaternion);

    Quaternion make_euler_rotation(const float3& rad);

    float4x4 billboard_matrix(float4x4 cameraMatrix);

    float2 world_to_screen(const float3& worldPos, const float4x4& viewMatrix, const float4x4& projMatrix, std::uint32_t screenWidth, std::uint32_t screenHeight);

    float lerp(float start, float end, float t);

    // 移動成分のみ抽出
    float3 get_translation(const float4x4& matrix);

    // 球面線形補間 (Slerp) 関数
    float3 slerp(const float3& v1, const float3& v2, float t);

    // イージング
    namespace easing
    {
        // t: 0.0f ～ 1.0f の入力 (経過割合)
        // 戻り値: 0.0f ～ 1.0f の出力 (補間値)

        // 線形（普通のLerpと同じ）
        float linear(float t);

        // 二乗で加速（最初ゆっくり → 後半速い）
        float ease_in_quad(float t);

        // 二乗で減速（最初速い → 後半ゆっくり）
        float ease_out_quad(float t);

        // 二乗で加減速（最初ゆっくり → 中盤速い → 最後ゆっくり）
        float ease_in_out_quad(float t);
    }
} // Drama::Math 名前空間

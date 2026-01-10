#pragma once
// ChoMath.cpp : スタティック ライブラリ用の関数を定義します。
//

/*
行ベクトル　row-vector
行優先 row-major
*/

#include "Vector2.h"
#include "Vector3.h"
#include "Scale.h"
#include "Vector4.h"
#include "Quaternion.h"
#include "Matrix4.h"
#include "mathShapes.h"

#include <numbers>

namespace Drama::Math
{
    constexpr float PI = std::numbers::pi_v<float>;

    enum class RotationOrder
    {
        XYZ,
        YXZ,
        ZXY,
        ZYX,
        YZX,
        XZY
    };

    // SRT をまとめた構造体
    struct SRT final
    {
        float3 translation;
        float3 rotationEuler; // ラジアン or 度数法 に合わせる
        Scale scale;
    };

    template <class T>
    [[nodiscard]] constexpr T Clamp(T x, T min, T max) noexcept
    {
        if (max < min) { T t = min; min = max; max = t; }
        return (x < min) ? min : (x > max) ? max : x;
    }

    /// @brief 回転行列→クォータニオン（スケール除去＆数値ガード＆正規化付き）
    /// 行列 m は row-major で m[row][col] としてアクセス可能な想定。
    /// 3x3 部分に回転以外（スケール等）が混ざっていても列正規化である程度補正します。
    Quaternion FromMatrix(const float4x4& m) noexcept;

    [[nodiscard]] float3 TransformPoint(const float3& v, const float4x4& M) noexcept;

    [[nodiscard]] float3 TransformVector(const float3& v, const float4x4& M) noexcept;

    [[nodiscard]] float4x4 TranslateMatrix(const float3& t) noexcept;

    [[nodiscard]] float4x4 ScaleMatrix(float s) noexcept;

    [[nodiscard]] float4x4 ScaleMatrix(const float3& s) noexcept;

    [[nodiscard]] float4x4 ScaleMatrix(const Scale& s) noexcept;

    [[nodiscard]] float4x4 XAxisMatrix(float radian) noexcept;

    [[nodiscard]] float4x4 YAxisMatrix(float radian) noexcept;

    [[nodiscard]] float4x4 ZAxisMatrix(float radian) noexcept;

    [[nodiscard]] float4x4 RotateXYZMatrix(const float3& r) noexcept;

    [[nodiscard]] float4x4 ViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth) noexcept;

    [[nodiscard]] float4x4 PerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip) noexcept;

    [[nodiscard]] float4x4 OrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) noexcept;

    [[nodiscard]] float Normalize(float x, float min, float max) noexcept;

    /*=========== レガシー ===========*/

    float4x4 MakeRotateAxisAngle(const float3& axis, float angle);

    float4x4 DirectionToDirection(const float3& from, const float3& to);

    float DegreesToRadians(float degrees);

    float3 DegreesToRadians(const float3& degrees);;

    float RadiansToDegrees(float radians);

    float3 RadiansToDegrees(const float3& radians);

    Quaternion MakeRotateAxisAngleQuaternion(const float3& axis, float angle);

    float3 RotateVector(const float3& vector, const Quaternion& quaternion);

    float4x4 MakeRotateMatrix(const Quaternion& quaternion);;

    Quaternion FromEulerAngles(const float3& eulerAngles);

    float3 ToEulerAngles(const Quaternion& q, RotationOrder order);

    float4x4 MakeAffineMatrix(const float3& scale, const float3& rotate, const float3& translate);

    float4x4 MakeAffineMatrix(const Scale& scale, const Quaternion& rotate, const float3& translate);

    float3 TransformDirection(const float3& v, const float4x4& m);

    float LerpShortAngle(float startAngle, float endAngle, float t);

    Quaternion MakeLookRotation(const float3& forward, const float3& up);

    float3 GetForwardVectorFromMatrix(const float4x4& rotMatrix);

    SRT DecomposeMatrix(const float4x4& in);

    Quaternion MakeQuaternionRotation(const float3& rad, const float3& preRad, const Quaternion& quaternion);

    Quaternion MakeEulerRotation(const float3& rad);

    float4x4 BillboardMatrix(const float4x4 cameraMatrix);

    float2 WorldToScreen(const float3& worldPos, const float4x4& viewMatrix, const float4x4& projMatrix, uint32_t screenWidth, uint32_t screenHeight);

    float Lerp(float start, float end, float t);

    // 移動成分のみ抽出
    float3 GetTranslation(const float4x4& m);

    // 球面線形補間 (Slerp) 関数
    float3 Slerp(const float3& v1, const float3& v2, float t);

    // イージング
    namespace easing
    {
        // t: 0.0f ～ 1.0f の入力 (経過割合)
        // 戻り値: 0.0f ～ 1.0f の出力 (補間値)

        // 線形（普通のLerpと同じ）
        // 線形（普通のLerpと同じ）
        float Linear(float t);

        // 二乗で加速（最初ゆっくり → 後半速い）
        // 二乗で加速（最初ゆっくり → 後半速い）
        float EaseInQuad(float t);

        // 二乗で減速（最初速い → 後半ゆっくり）
        // 二乗で減速（最初速い → 後半ゆっくり）
        float EaseOutQuad(float t);

        // 二乗で加減速（最初ゆっくり → 中盤速い → 最後ゆっくり）
        // 二乗で加減速（最初ゆっくり → 中盤速い → 最後ゆっくり）
        float EaseInOutQuad(float t);
    }
};

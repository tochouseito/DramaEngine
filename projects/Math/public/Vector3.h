#pragma once
#include "MathStructAllowedList.h"
#include <cstdint>
#include <cmath>
#include <limits>
#include <type_traits>
#ifdef _DEBUG
#include <cassert>
#endif

namespace Drama::Math
{
    /// @brief 3次元ベクトル
    template<AllowedVector T>
    struct Vector3 final
    {
        /// @brief 別名アクセス
        union { struct { T x, y, z; }; struct { T r, g, b; }; T v[3]; };

        /*================ コンストラクタ ================*/
        /// @brief デフォルトコンストラクタ
        constexpr Vector3() noexcept : x(static_cast<T>(0)), y(static_cast<T>(0)), z(static_cast<T>(0)) {}
        /// @brief 引数付きコンストラクタ
        constexpr Vector3(T x_, T y_, T z_) noexcept : x(x_), y(y_), z(z_) {}

        /*================ 判定/初期化 ================*/
        /// @brief ゼロベクトルか判定
        constexpr bool IsZero() const noexcept { return x == static_cast<T>(0) && y == static_cast<T>(0) && z == static_cast<T>(0); }
        /// @brief ゼロ初期化
        constexpr void Initialize() noexcept { x = y = z = static_cast<T>(0); }

        /*================ 変換演算子 ================*/
        /// @brief bool型への変換（ゼロでなければtrue）
        explicit constexpr operator bool() const noexcept { return !IsZero(); }

        /*================ 配列アクセス ================*/
        /// @brief 配列アクセス（読み書き）0:x(=r),1:y(=g),2:z(=b)
        constexpr       T& operator[](std::size_t i) noexcept
        {
#ifdef _DEBUG
            assert(i < 3);
#endif
            return v[i];
        }
        /// @brief 配列アクセス（読み取り専用）0:x(=r),1:y(=g),2:z(=b)
        constexpr const T& operator[](std::size_t i) const noexcept
        {
#ifdef _DEBUG
            assert(i < 3);
#endif
            return v[i];
        }

        /*================ 符号演算子 ================*/
        /// @brief 単項プラス
        constexpr Vector3 operator+() const noexcept { return *this; }
        /// @brief 単項マイナス
        constexpr Vector3 operator-() const noexcept { return { -x, -y, -z }; }

        /*================ 二項演算子 ================*/
        /// @brief 加算
        constexpr Vector3 operator+(const Vector3& o) const noexcept { return { x + o.x, y + o.y, z + o.z }; }
        /// @brief 減算
        constexpr Vector3 operator-(const Vector3& o) const noexcept { return { x - o.x, y - o.y, z - o.z }; }
        /// @brief スカラー乗算
        constexpr Vector3 operator*(T s) const noexcept { return { x * s, y * s, z * s }; }
        /// @brief スカラー除算
        constexpr Vector3 operator/(T s) const noexcept { return { x / s, y / s, z / s }; }

        /*================ 複合代入演算子 ================*/
        /// @brief 加算代入
        constexpr Vector3& operator+=(const Vector3& o) noexcept { x += o.x; y += o.y; z += o.z; return *this; }
        /// @brief 減算代入
        constexpr Vector3& operator-=(const Vector3& o) noexcept { x -= o.x; y -= o.y; z -= o.z; return *this; }
        /// @brief 乗算代入
        constexpr Vector3& operator*=(T s) noexcept { x *= s; y *= s; z *= s; return *this; }
        /// @brief 除算代入
        constexpr Vector3& operator/=(T s) noexcept { x /= s; y /= s; z /= s; return *this; }

        /*================ インクリメント/デクリメント ================*/
        /// @brief 前置インクリメント
        constexpr Vector3& operator++() noexcept { ++x; ++y; ++z; return *this; }
        /// @brief 後置インクリメント
        constexpr Vector3 operator++(int) noexcept { Vector3 t = *this; ++(*this); return t; }
        /// @brief 前置デクリメント
        constexpr Vector3& operator--() noexcept { --x; --y; --z; return *this; }
        /// @brief 後置デクリメント
        constexpr Vector3 operator--(int) noexcept { Vector3 t = *this; --(*this); return t; }

        /*================ 比較演算子 ================*/
        /// @brief 等価（全成分一致）
        constexpr bool operator==(const Vector3& o) const noexcept { return x == o.x && y == o.y && z == o.z; }
        /// @brief 非等価
        constexpr bool operator!=(const Vector3& o) const noexcept { return !(*this == o); }
        /// @brief 小なり（全成分）
        constexpr bool operator<(const Vector3& o) const noexcept { return (x < o.x) && (y < o.y) && (z < o.z); }
        /// @brief 小なりイコール（全成分）
        constexpr bool operator<=(const Vector3& o) const noexcept { return (x <= o.x) && (y <= o.y) && (z <= o.z); }
        /// @brief 大なり（全成分）
        constexpr bool operator>(const Vector3& o) const noexcept { return (x > o.x) && (y > o.y) && (z > o.z); }
        /// @brief 大なりイコール（全成分）
        constexpr bool operator>=(const Vector3& o) const noexcept { return (x >= o.x) && (y >= o.y) && (z >= o.z); }

        /*================ 計算メンバ関数 ================*/
        /// @brief 長さ
        T Length() const noexcept { return static_cast<T>(std::sqrt(x * x + y * y + z * z)); }
        /// @brief 長さの二乗
        constexpr T LengthSq() const noexcept { return x * x + y * y + z * z; }
        /// @brief 正規化
        Vector3& Normalize() noexcept { T l = Length(); if (l != static_cast<T>(0)) { x /= l; y /= l; z /= l; } return *this; }
        /// @brief 内積
        constexpr T Dot(const Vector3& o) const noexcept { return x * o.x + y * o.y + z * o.z; }
        /// @brief 外積
        constexpr Vector3 Cross(const Vector3& o) const noexcept { return { y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x }; }

        /*================ Epsilon 比較 ================*/
        /// @brief Epsilonを用いた等価判定（全成分）
        constexpr bool EqualsEpsilon(const Vector3& o, T e) const noexcept
        {
            if constexpr (std::is_floating_point_v<T>) { auto ab = [](T v) { return v >= T(0) ? v : -v; }; return ab(x - o.x) <= e && ab(y - o.y) <= e && ab(z - o.z) <= e; }
            else { return (*this == o); }
        }
        /// @brief 既定Epsilonでの等価判定（浮動小数点は 10*epsilon）
        constexpr bool EqualsEpsilon(const Vector3& o) const noexcept
        {
            if constexpr (std::is_floating_point_v<T>) { return EqualsEpsilon(o, T(10) * std::numeric_limits<T>::epsilon()); }
            else { return (*this == o); }
        }

        /*================ ユーティリティ ================*/
        /// @brief スカラー×ベクトル（左掛け）
        friend constexpr Vector3 operator*(T s, const Vector3& v) noexcept { return v * s; }

        /*================ 静的メンバ関数 ================*/
        /// @brief 零ベクトル取得
        static constexpr Vector3 Zero() noexcept { return { static_cast<T>(0), static_cast<T>(0), static_cast<T>(0) }; }
        /// @brief 単位ベクトル取得
        static constexpr Vector3 One() noexcept { return { static_cast<T>(1), static_cast<T>(1), static_cast<T>(1) }; }
        /// @brief 単位Xベクトル取得
        static constexpr Vector3 UnitX() noexcept { return { static_cast<T>(1), static_cast<T>(0), static_cast<T>(0) }; }
        /// @brief 単位Yベクトル取得
        static constexpr Vector3 UnitY() noexcept { return { static_cast<T>(0), static_cast<T>(1), static_cast<T>(0) }; }
        /// @brief 単位Zベクトル取得
        static constexpr Vector3 UnitZ() noexcept { return { static_cast<T>(0), static_cast<T>(0), static_cast<T>(1) }; }
        /// @brief 最大値ベクトル取得
        static constexpr Vector3 MaxValue() noexcept { return { std::numeric_limits<T>::max(), std::numeric_limits<T>::max(), std::numeric_limits<T>::max() }; }
        /// @brief 最小値ベクトル取得
        static constexpr Vector3 MinValue() noexcept { return { std::numeric_limits<T>::lowest(), std::numeric_limits<T>::lowest(), std::numeric_limits<T>::lowest() }; }
        /// @brief 正規化済みベクトル取得
        static Vector3 Normalize(const Vector3& v) noexcept
        {
            Vector3 result = v;
            result.Normalize();
            return result;
        }
        /// @brief 内積計算
        static constexpr T Dot(const Vector3& a, const Vector3& b) noexcept
        {
            return a.x * b.x + a.y * b.y + a.z * b.z;
        }
        /// @brief 外積計算
        static constexpr Vector3 Cross(const Vector3& a, const Vector3& b) noexcept
        {
            return {
                a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x
            };
        }
        /// @brief 線形補間
        /// @param a 
        /// @param b 
        /// @param t 
        /// @return 
        static constexpr Vector3 Lerp(const Vector3& a, const Vector3& b, float t) noexcept
        {
            return a + (b - a) * static_cast<T>(t);
        }
    };

    using int3 = Vector3<int>;
    using uint3 = Vector3<unsigned int>;
    using uint32_t3 = Vector3<uint32_t>;
    using float3 = Vector3<float>;
    using double3 = Vector3<double>;
} // namespace math

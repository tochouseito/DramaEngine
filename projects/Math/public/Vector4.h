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
    /// @brief 4次元ベクトル
    template<AllowedVector T>
    struct Vector4 final
    {
        /// @brief 別名アクセス
        union
        {
            struct { T x, y, z, w; };
            struct { T r, g, b, a; };
            T v[4];
        };

        /*================ コンストラクタ ================*/
        /// @brief デフォルトコンストラクタ
        constexpr Vector4() noexcept : x(static_cast<T>(0)), y(static_cast<T>(0)), z(static_cast<T>(0)), w(static_cast<T>(0)) {}
        /// @brief 引数付きコンストラクタ
        constexpr Vector4(T x_, T y_, T z_, T w_) noexcept : x(x_), y(y_), z(z_), w(w_) {}

        /*================ 判定/初期化 ================*/
        /// @brief ゼロベクトルか判定
        constexpr bool IsZero() const noexcept
        {
            return x == static_cast<T>(0) && y == static_cast<T>(0) && z == static_cast<T>(0) && w == static_cast<T>(0);
        }
        /// @brief ゼロ初期化
        constexpr void Initialize() noexcept { x = y = z = w = static_cast<T>(0); }

        /*================ 変換演算子 ================*/
        /// @brief bool型への変換（ゼロでなければtrue）
        explicit constexpr operator bool() const noexcept { return !IsZero(); }

        /*================ 配列アクセス ================*/
        /// @brief 配列アクセス（読み書き）0:x(=r),1:y(=g),2:z(=b),3:w(=a)
        constexpr       T& operator[](std::size_t i) noexcept
        {
#ifdef _DEBUG
            assert(i < 4);
#endif
            return v[i];
        }
        /// @brief 配列アクセス（読み取り専用）0:x(=r),1:y(=g),2:z(=b),3:w(=a)
        constexpr const T& operator[](std::size_t i) const noexcept
        {
#ifdef _DEBUG
            assert(i < 4);
#endif
            return v[i];
        }

        /*================ 符号演算子 ================*/
        /// @brief 単項プラス
        constexpr Vector4 operator+() const noexcept { return *this; }
        /// @brief 単項マイナス
        constexpr Vector4 operator-() const noexcept { return { -x, -y, -z, -w }; }

        /*================ 二項演算子 ================*/
        /// @brief 加算
        constexpr Vector4 operator+(const Vector4& o) const noexcept { return { x + o.x, y + o.y, z + o.z, w + o.w }; }
        /// @brief 減算
        constexpr Vector4 operator-(const Vector4& o) const noexcept { return { x - o.x, y - o.y, z - o.z, w - o.w }; }
        /// @brief スカラー乗算
        constexpr Vector4 operator*(T s) const noexcept { return { x * s, y * s, z * s, w * s }; }
        /// @brief スカラー除算
        constexpr Vector4 operator/(T s) const noexcept { return { x / s, y / s, z / s, w / s }; }

        /*================ 複合代入演算子 ================*/
        /// @brief 加算代入
        constexpr Vector4& operator+=(const Vector4& o) noexcept { x += o.x; y += o.y; z += o.z; w += o.w; return *this; }
        /// @brief 減算代入
        constexpr Vector4& operator-=(const Vector4& o) noexcept { x -= o.x; y -= o.y; z -= o.z; w -= o.w; return *this; }
        /// @brief 乗算代入
        constexpr Vector4& operator*=(T s) noexcept { x *= s; y *= s; z *= s; w *= s; return *this; }
        /// @brief 除算代入
        constexpr Vector4& operator/=(T s) noexcept { x /= s; y /= s; z /= s; w /= s; return *this; }

        /*================ インクリメント/デクリメント ================*/
        /// @brief 前置インクリメント
        constexpr Vector4& operator++() noexcept { ++x; ++y; ++z; ++w; return *this; }
        /// @brief 後置インクリメント
        constexpr Vector4 operator++(int) noexcept { Vector4 t = *this; ++(*this); return t; }
        /// @brief 前置デクリメント
        constexpr Vector4& operator--() noexcept { --x; --y; --z; --w; return *this; }
        /// @brief 後置デクリメント
        constexpr Vector4 operator--(int) noexcept { Vector4 t = *this; --(*this); return t; }

        /*================ 比較演算子 ================*/
        /// @brief 等価（全成分一致）
        constexpr bool operator==(const Vector4& o) const noexcept { return x == o.x && y == o.y && z == o.z && w == o.w; }
        /// @brief 非等価
        constexpr bool operator!=(const Vector4& o) const noexcept { return !(*this == o); }
        /// @brief 小なり（全成分）
        constexpr bool operator<(const Vector4& o) const noexcept { return (x < o.x) && (y < o.y) && (z < o.z) && (w < o.w); }
        /// @brief 小なりイコール（全成分）
        constexpr bool operator<=(const Vector4& o) const noexcept { return (x <= o.x) && (y <= o.y) && (z <= o.z) && (w <= o.w); }
        /// @brief 大なり（全成分）
        constexpr bool operator>(const Vector4& o) const noexcept { return (x > o.x) && (y > o.y) && (z > o.z) && (w > o.w); }
        /// @brief 大なりイコール（全成分）
        constexpr bool operator>=(const Vector4& o) const noexcept { return (x >= o.x) && (y >= o.y) && (z >= o.z) && (w >= o.w); }

        /*================ 計算メンバ関数 ================*/
        /// @brief 長さ
        T Length() const noexcept { return static_cast<T>(std::sqrt(x * x + y * y + z * z + w * w)); }
        /// @brief 長さの二乗
        constexpr T LengthSq() const noexcept { return x * x + y * y + z * z + w * w; }
        /// @brief 正規化
        Vector4& Normalize() noexcept { T l = Length(); if (l != static_cast<T>(0)) { x /= l; y /= l; z /= l; w /= l; } return *this; }
        /// @brief 内積
        constexpr T Dot(const Vector4& o) const noexcept { return x * o.x + y * o.y + z * o.z + w * o.w; }
        /// @brief 外積（4次元ベクトルの外積は定義されていないため、3次元ベクトルとして計算し、w成分は0に設定）
        constexpr Vector4 Cross(const Vector4& o) const noexcept
        {
            return {
                y * o.z - z * o.y,
                z * o.x - x * o.z,
                x * o.y - y * o.x,
                static_cast<T>(0)
            };
        }

        /*================ Epsilon 比較 ================*/
        /// @brief Epsilonを用いた等価判定（全成分）
        constexpr bool EqualsEpsilon(const Vector4& o, T e) const noexcept
        {
            if constexpr (std::is_floating_point_v<T>)
            {
                auto ab = [](T v) { return v >= T(0) ? v : -v; };
                return ab(x - o.x) <= e && ab(y - o.y) <= e && ab(z - o.z) <= e && ab(w - o.w) <= e;
            }
            else
            {
                return (*this == o);
            }
        }
        /// @brief 既定Epsilonでの等価判定（浮動小数点は 10*epsilon）
        constexpr bool EqualsEpsilon(const Vector4& o) const noexcept
        {
            if constexpr (std::is_floating_point_v<T>) { return EqualsEpsilon(o, T(10) * std::numeric_limits<T>::epsilon()); }
            else { return (*this == o); }
        }

        /*================ ユーティリティ ================*/
        /// @brief スカラー×ベクトル（左掛け）
        friend constexpr Vector4 operator*(T s, const Vector4& v) noexcept { return v * s; }

        /*================ 静的メンバ関数 ================*/
        /// @brief 零ベクトル取得
        static constexpr Vector4 Zero() noexcept { return { static_cast<T>(0), static_cast<T>(0), static_cast<T>(0), static_cast<T>(0) }; }
        /// @brief 単位ベクトル取得
        static constexpr Vector4 One() noexcept { return { static_cast<T>(1), static_cast<T>(1), static_cast<T>(1), static_cast<T>(1) }; }
        /// @brief 単位Xベクトル取得
        static constexpr Vector4 UnitX() noexcept { return { static_cast<T>(1), static_cast<T>(0), static_cast<T>(0), static_cast<T>(0) }; }
        /// @brief 単位Yベクトル取得
        static constexpr Vector4 UnitY() noexcept { return { static_cast<T>(0), static_cast<T>(1), static_cast<T>(0), static_cast<T>(0) }; }
        /// @brief 単位Zベクトル取得
        static constexpr Vector4 UnitZ() noexcept { return { static_cast<T>(0), static_cast<T>(0), static_cast<T>(1), static_cast<T>(0) }; }
        /// @brief 単位Wベクトル取得
        static constexpr Vector4 UnitW() noexcept { return { static_cast<T>(0), static_cast<T>(0), static_cast<T>(0), static_cast<T>(1) }; }
        /// @brief 最大値ベクトル取得
        static constexpr Vector4 MaxValue() noexcept { return { std::numeric_limits<T>::max(), std::numeric_limits<T>::max(), std::numeric_limits<T>::max(), std::numeric_limits<T>::max() }; }
        /// @brief 最小値ベクトル取得
        static constexpr Vector4 MinValue() noexcept { return { std::numeric_limits<T>::lowest(), std::numeric_limits<T>::lowest(), std::numeric_limits<T>::lowest(), std::numeric_limits<T>::lowest() }; }
        /// @brief 正規化済みベクトル取得
        static Vector4 Normalize(const Vector4& v) noexcept
        {
            Vector4 result = v;
            result.Normalize();
            return result;
        }
        /// @brief 内積計算
        static constexpr T Dot(const Vector4& v0, const Vector4& v1) noexcept
        {
            return v0.Dot(v1);
        }
        /// @brief 外積計算（4次元ベクトルの外積は定義されていないため、3次元ベクトルとして計算し、w成分は0に設定）
        static constexpr Vector4 Cross(const Vector4& v0, const Vector4& v1) noexcept
        {
            return v0.Cross(v1);
        }
    };

    using int4 = Vector4<int>;
    using uint4 = Vector4<unsigned int>;
    using uint32_t4 = Vector4<uint32_t>;
    using float4 = Vector4<float>;
    using double4 = Vector4<double>;
} // namespace math

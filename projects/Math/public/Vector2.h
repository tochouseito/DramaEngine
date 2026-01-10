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
    /// @brief 2次元ベクトル
    template<AllowedVector T>
    struct Vector2 final
    {
        /// @brief 別名アクセス
        union { struct { T x, y; }; struct { T r, g; }; T v[2]; };

        /*================ コンストラクタ ================*/
        /// @brief デフォルトコンストラクタ
        constexpr Vector2() noexcept : x(static_cast<T>(0)), y(static_cast<T>(0)) {}
        /// @brief 引数付きコンストラクタ
        constexpr Vector2(T x_, T y_) noexcept : x(x_), y(y_) {}

        /*================ 判定/初期化 ================*/
        /// @brief ゼロベクトルか判定
        constexpr bool IsZero() const noexcept { return x == static_cast<T>(0) && y == static_cast<T>(0); }
        /// @brief ゼロ初期化
        constexpr void Initialize() noexcept { x = y = static_cast<T>(0); }

        /*================ 変換演算子 ================*/
        /// @brief bool型への変換（ゼロでなければtrue）
        explicit constexpr operator bool() const noexcept { return !IsZero(); }

        /*================ 配列アクセス ================*/
        /// @brief 配列アクセス（読み書き）0:x(=r),1:y(=g)
        constexpr       T& operator[](std::size_t i) noexcept
        {
#ifdef _DEBUG
            assert(i < 2);
#endif
            return v[i];
        }
        /// @brief 配列アクセス（読み取り専用）0:x(=r),1:y(=g)
        constexpr const T& operator[](std::size_t i) const noexcept
        {
#ifdef _DEBUG
            assert(i < 2);
#endif
            return v[i];
        }

        /*================ 符号演算子 ================*/
        /// @brief 単項プラス
        constexpr Vector2 operator+() const noexcept { return *this; }
        /// @brief 単項マイナス
        constexpr Vector2 operator-() const noexcept { return { -x, -y }; }

        /*================ 二項演算子 ================*/
        /// @brief 加算
        constexpr Vector2 operator+(const Vector2& o) const noexcept { return { x + o.x, y + o.y }; }
        /// @brief 減算
        constexpr Vector2 operator-(const Vector2& o) const noexcept { return { x - o.x, y - o.y }; }
        /// @brief スカラー乗算
        constexpr Vector2 operator*(T s) const noexcept { return { x * s, y * s }; }
        /// @brief スカラー除算
        constexpr Vector2 operator/(T s) const noexcept { return { x / s, y / s }; }

        /*================ 複合代入演算子 ================*/
        /// @brief 加算代入
        constexpr Vector2& operator+=(const Vector2& o) noexcept { x += o.x; y += o.y; return *this; }
        /// @brief 減算代入
        constexpr Vector2& operator-=(const Vector2& o) noexcept { x -= o.x; y -= o.y; return *this; }
        /// @brief 乗算代入
        constexpr Vector2& operator*=(T s) noexcept { x *= s; y *= s; return *this; }
        /// @brief 除算代入
        constexpr Vector2& operator/=(T s) noexcept { x /= s; y /= s; return *this; }

        /*================ インクリメント/デクリメント ================*/
        /// @brief 前置インクリメント
        constexpr Vector2& operator++() noexcept { ++x; ++y; return *this; }
        /// @brief 後置インクリメント
        constexpr Vector2 operator++(int) noexcept { Vector2 t = *this; ++(*this); return t; }
        /// @brief 前置デクリメント
        constexpr Vector2& operator--() noexcept { --x; --y; return *this; }
        /// @brief 後置デクリメント
        constexpr Vector2 operator--(int) noexcept { Vector2 t = *this; --(*this); return t; }

        /*================ 比較演算子 ================*/
        /// @brief 等価（全成分一致）
        constexpr bool operator==(const Vector2& o) const noexcept { return x == o.x && y == o.y; }
        /// @brief 非等価
        constexpr bool operator!=(const Vector2& o) const noexcept { return !(*this == o); }
        /// @brief 小なり（全成分）
        constexpr bool operator<(const Vector2& o) const noexcept { return (x < o.x) && (y < o.y); }
        /// @brief 小なりイコール（全成分）
        constexpr bool operator<=(const Vector2& o) const noexcept { return (x <= o.x) && (y <= o.y); }
        /// @brief 大なり（全成分）
        constexpr bool operator>(const Vector2& o) const noexcept { return (x > o.x) && (y > o.y); }
        /// @brief 大なりイコール（全成分）
        constexpr bool operator>=(const Vector2& o) const noexcept { return (x >= o.x) && (y >= o.y); }

        /*================ 計算メンバ関数 ================*/
        /// @brief 長さ
        T Length() const noexcept { return static_cast<T>(std::sqrt(x * x + y * y)); }
        /// @brief 長さの二乗
        constexpr T LengthSq() const noexcept { return x * x + y * y; }
        /// @brief 正規化
        Vector2& Normalize() noexcept { T l = Length(); if (l != static_cast<T>(0)) { x /= l; y /= l; } return *this; }
        /// @brief 内積
        constexpr T Dot(const Vector2& o) const noexcept { return x * o.x + y * o.y; }

        /*================ Epsilon 比較 ================*/
        /// @brief Epsilonを用いた等価判定（全成分）
        constexpr bool EqualsEpsilon(const Vector2& o, T e) const noexcept
        {
            if constexpr (std::is_floating_point_v<T>) { auto ab = [](T v) { return v >= T(0) ? v : -v; }; return ab(x - o.x) <= e && ab(y - o.y) <= e; }
            else { return (*this == o); }
        }
        /// @brief 既定Epsilonでの等価判定（浮動小数点は 10*epsilon）
        constexpr bool EqualsEpsilon(const Vector2& o) const noexcept
        {
            if constexpr (std::is_floating_point_v<T>) { return EqualsEpsilon(o, T(10) * std::numeric_limits<T>::epsilon()); }
            else { return (*this == o); }
        }

        /*================ ユーティリティ ================*/
        /// @brief スカラー×ベクトル（左掛け）
        friend constexpr Vector2 operator*(T s, const Vector2& v) noexcept { return v * s; }

        /*================ 静的メンバ関数 ================*/
        /// @brief 零ベクトル取得
        static constexpr Vector2 Zero() noexcept { return { static_cast<T>(0), static_cast<T>(0) }; }
        /// @brief 単位ベクトル取得
        static constexpr Vector2 One() noexcept { return { static_cast<T>(1), static_cast<T>(1) }; }
        /// @brief 単位Xベクトル取得
        static constexpr Vector2 UnitX() noexcept { return { static_cast<T>(1), static_cast<T>(0) }; }
        /// @brief 単位Yベクトル取得
        static constexpr Vector2 UnitY() noexcept { return { static_cast<T>(0), static_cast<T>(1) }; }
        /// @brief 最大値ベクトル取得
        static constexpr Vector2 MaxValue() noexcept { return { std::numeric_limits<T>::max(), std::numeric_limits<T>::max() }; }
        /// @brief 最小値ベクトル取得
        static constexpr Vector2 MinValue() noexcept { return { std::numeric_limits<T>::lowest(), std::numeric_limits<T>::lowest() }; }
        /// @brief 正規化済みベクトル取得
        static Vector2 Normalize(const Vector2& v) noexcept
        {
            Vector2 result = v;
            result.Normalize();
            return result;
        }
        /// @brief 内積計算
        static constexpr T Dot(const Vector2& v1, const Vector2& v2) noexcept
        {
            return v1.Dot(v2);
        }
    };

    using int2 = Vector2<int>;
    using uint2 = Vector2<unsigned int>;
    using uint32_t2 = Vector2<uint32_t>;
    using float2 = Vector2<float>;
    using double2 = Vector2<double>;
}// namespace math

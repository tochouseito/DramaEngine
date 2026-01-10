#pragma once
#include <cstdint>
#include <cmath>
#include <limits>
#ifdef _DEBUG
#include <cassert>
#endif

namespace Drama::Math
{
    /// @brief スケール構造体
    struct Scale final
    {
        /// @brief 別名アクセス
        union
        {
            struct { float x, y, z; };
            struct { float r, g, b; };
            float v[3];
        };

        /*================ コンストラクタ ================*/
        /// @brief デフォルトコンストラクタ
        constexpr Scale() noexcept : x(static_cast<float>(1)), y(static_cast<float>(1)), z(static_cast<float>(1)) {}
        /// @brief 引数付きコンストラクタ
        constexpr Scale(float x_, float y_, float z_) noexcept : x(x_), y(y_), z(z_) {}

        /*================ 判定/初期化 ================*/
        /// @brief ゼロベクトルか判定
        constexpr bool IsZero() const noexcept { return x == static_cast<float>(0) && y == static_cast<float>(0) && z == static_cast<float>(0); }
        /// @brief ゼロ初期化
        constexpr void Initialize() noexcept { x = y = z = static_cast<float>(1); }

        /*================ 変換演算子 ================*/
        /// @brief bool型への変換（ゼロでなければtrue）
        explicit constexpr operator bool() const noexcept { return !IsZero(); }

        /*================ 配列アクセス ================*/
        /// @brief 配列アクセス（読み書き）0:x(=r),1:y(=g),2:z(=b)
        constexpr       float& operator[](std::size_t i) noexcept
        {
#ifdef _DEBUG
            assert(i < 3);
#endif
            return v[i];
        }
        /// @brief 配列アクセス（読み取り専用）0:x(=r),1:y(=g),2:z(=b)
        constexpr const float& operator[](std::size_t i) const noexcept
        {
#ifdef _DEBUG
            assert(i < 3);
#endif
            return v[i];
        }

        /*================ 符号演算子 ================*/
        /// @brief 単項プラス
        constexpr Scale operator+() const noexcept { return *this; }
        /// @brief 単項マイナス
        constexpr Scale operator-() const noexcept { return { -x, -y, -z }; }

        /*================ 二項演算子 ================*/
        /// @brief 加算
        constexpr Scale operator+(const Scale& o) const noexcept { return { x + o.x, y + o.y, z + o.z }; }
        /// @brief 減算
        constexpr Scale operator-(const Scale& o) const noexcept { return { x - o.x, y - o.y, z - o.z }; }
        /// @brief スカラー乗算
        constexpr Scale operator*(float s) const noexcept { return { x * s, y * s, z * s }; }
        /// @brief スカラー除算
        constexpr Scale operator/(float s) const noexcept { return { x / s, y / s, z / s }; }

        /*================ 複合代入演算子 ================*/
        /// @brief 加算代入
        constexpr Scale& operator+=(const Scale& o) noexcept { x += o.x; y += o.y; z += o.z; return *this; }
        /// @brief 減算代入
        constexpr Scale& operator-=(const Scale& o) noexcept { x -= o.x; y -= o.y; z -= o.z; return *this; }
        /// @brief 乗算代入
        constexpr Scale& operator*=(float s) noexcept { x *= s; y *= s; z *= s; return *this; }
        /// @brief 除算代入
        constexpr Scale& operator/=(float s) noexcept { x /= s; y /= s; z /= s; return *this; }

        /*================ インクリメント/デクリメント ================*/
        /// @brief 前置インクリメント
        constexpr Scale& operator++() noexcept { ++x; ++y; ++z; return *this; }
        /// @brief 後置インクリメント
        constexpr Scale operator++(int) noexcept { Scale t = *this; ++(*this); return t; }
        /// @brief 前置デクリメント
        constexpr Scale& operator--() noexcept { --x; --y; --z; return *this; }
        /// @brief 後置デクリメント
        constexpr Scale operator--(int) noexcept { Scale t = *this; --(*this); return t; }

        /*================ 比較演算子 ================*/
        /// @brief 等価（全成分一致）
        constexpr bool operator==(const Scale& o) const noexcept { return x == o.x && y == o.y && z == o.z; }
        /// @brief 非等価
        constexpr bool operator!=(const Scale& o) const noexcept { return !(*this == o); }
        /// @brief 小なり（全成分）
        constexpr bool operator<(const Scale& o) const noexcept { return (x < o.x) && (y < o.y) && (z < o.z); }
        /// @brief 小なりイコール（全成分）
        constexpr bool operator<=(const Scale& o) const noexcept { return (x <= o.x) && (y <= o.y) && (z <= o.z); }
        /// @brief 大なり（全成分）
        constexpr bool operator>(const Scale& o) const noexcept { return (x > o.x) && (y > o.y) && (z > o.z); }
        /// @brief 大なりイコール（全成分）
        constexpr bool operator>=(const Scale& o) const noexcept { return (x >= o.x) && (y >= o.y) && (z >= o.z); }

        /*================ 計算メンバ関数 ================*/
        /// @brief 長さ
        float Length() const noexcept { return static_cast<float>(std::sqrt(x * x + y * y + z * z)); }
        /// @brief 長さの二乗
        constexpr float LengthSq() const noexcept { return x * x + y * y + z * z; }
        /// @brief 正規化
        Scale& Normalize() noexcept { float l = Length(); if (l != static_cast<float>(0)) { x /= l; y /= l; z /= l; } return *this; }
        /// @brief 内積
        constexpr float Dot(const Scale& o) const noexcept { return x * o.x + y * o.y + z * o.z; }
        /// @brief 外積
        constexpr Scale Cross(const Scale& o) const noexcept { return { y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x }; }

        /*================ Epsilon 比較 ================*/
        /// @brief Epsilonを用いた等価判定（全成分）
        constexpr bool EqualsEpsilon(const Scale& o, float e) const noexcept
        {
            if constexpr (std::is_floating_point_v<float>) { auto ab = [](float v_) { return v_ >= float(0) ? v_ : -v_; }; return ab(x - o.x) <= e && ab(y - o.y) <= e && ab(z - o.z) <= e; }
            else { return (*this == o); }
        }
        /// @brief 既定Epsilonでの等価判定（浮動小数点は 10*epsilon）
        constexpr bool EqualsEpsilon(const Scale& o) const noexcept
        {
            if constexpr (std::is_floating_point_v<float>) { return EqualsEpsilon(o, float(10) * std::numeric_limits<float>::epsilon()); }
            else { return (*this == o); }
        }

        /*================ ユーティリティ ================*/
        /// @brief スカラー×ベクトル（左掛け）
        friend constexpr Scale operator*(float s, const Scale& v) noexcept { return v * s; }

        /*================ 静的メンバ関数 ================*/
        /// @brief 零ベクトル取得
        static constexpr Scale Zero() noexcept { return { static_cast<float>(0), static_cast<float>(0), static_cast<float>(0) }; }
        /// @brief 単位ベクトル取得
        static constexpr Scale One() noexcept { return { static_cast<float>(1), static_cast<float>(1), static_cast<float>(1) }; }
        /// @brief 単位Xベクトル取得
        static constexpr Scale UnitX() noexcept { return { static_cast<float>(1), static_cast<float>(0), static_cast<float>(0) }; }
        /// @brief 単位Yベクトル取得
        static constexpr Scale UnitY() noexcept { return { static_cast<float>(0), static_cast<float>(1), static_cast<float>(0) }; }
        /// @brief 単位Zベクトル取得
        static constexpr Scale UnitZ() noexcept { return { static_cast<float>(0), static_cast<float>(0), static_cast<float>(1) }; }
        /// @brief 最大値ベクトル取得
        static constexpr Scale MaxValue() noexcept { return { std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() }; }
        /// @brief 最小値ベクトル取得
        static constexpr Scale MinValue() noexcept { return { std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() }; }
        /// @brief 正規化済みベクトル取得
        static Scale Normalize(const Scale& v) noexcept
        {
            Scale result = v;
            result.Normalize();
            return result;
        }
        /// @brief 内積計算
        static constexpr float Dot(const Scale& a, const Scale& b) noexcept
        {
            return a.x * b.x + a.y * b.y + a.z * b.z;
        }
        /// @brief 外積計算
        static constexpr Scale Cross(const Scale& a, const Scale& b) noexcept
        {
            return {
                a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x
            };
        }
        /// @brief 線形補間
        /// @param start 
        /// @param end 
        /// @param t 
        /// @return 
        static Scale Lerp(const Scale& start, const Scale& end, float t) noexcept
        {
            return {
                start.x + (end.x - start.x) * t,
                start.y + (end.y - start.y) * t,
                start.z + (end.z - start.z) * t
            };
        }
    };
} // namespace math

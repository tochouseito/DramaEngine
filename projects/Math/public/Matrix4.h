#pragma once
#include "MathStructAllowedList.h"
#include <cstdint>
#include <cmath>
#include <limits>
#include <type_traits>
#include <algorithm>  // std::swap
#ifdef _DEBUG
#include <cassert>
#endif

namespace Drama::Math
{
    /// @brief 4x4行列（row-major: m[row][col]）
    template<AllowedVector T>
    struct Matrix4 final
    {
        float m[4][4] = {};

        /// @brief 単位行列で初期化
        constexpr void InitializeIdentity() noexcept
        {
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    m[i][j] = (i == j) ? T{ 1 } : T{ 0 };
        }

        /// @brief 単位行列生成
        [[nodiscard]] static constexpr Matrix4 Identity() noexcept
        {
            Matrix4 a; a.InitializeIdentity(); return a;
        }

        /// @brief ゼロ行列で初期化
        constexpr void InitializeZero() noexcept
        {
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    m[i][j] = T{ 0 };
        }

        /// @brief ゼロ行列生成
        [[nodiscard]] static constexpr Matrix4 Zero() noexcept
        {
            Matrix4 a; a.InitializeZero(); return a;
        }

        /// @brief 行列の転置（破壊的）
        constexpr void Transpose() noexcept
        {
            for (int i = 0; i < 4; ++i)
                for (int j = i + 1; j < 4; ++j)
                    std::swap(m[i][j], m[j][i]);
        }

        /// @brief 転置（非破壊）
        [[nodiscard]] static constexpr Matrix4 Transpose(const Matrix4& a) noexcept
        {
            Matrix4 r;
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    r.m[i][j] = a.m[j][i];
            return r;
        }

        /// @brief 行列積（r = this * o）
        [[nodiscard]] constexpr Matrix4 Multiply(const Matrix4& o) const noexcept
        {
            Matrix4 r{};
            for (int i = 0; i < 4; ++i)
            {
                for (int k = 0; k < 4; ++k)
                {
                    const T aik = m[i][k];
                    // i行とoのk列でFMA風に加算（軽い最適化）
                    r.m[i][0] += aik * o.m[k][0];
                    r.m[i][1] += aik * o.m[k][1];
                    r.m[i][2] += aik * o.m[k][2];
                    r.m[i][3] += aik * o.m[k][3];
                }
            }
            return r;
        }

        [[nodiscard]] constexpr Matrix4 operator*(const Matrix4& o) const noexcept { return Multiply(o); }

        // --- スカラー演算 ---

        [[nodiscard]] constexpr Matrix4 operator+(const Matrix4& o) const noexcept
        {
            Matrix4 r;
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    r.m[i][j] = m[i][j] + o.m[i][j];
            return r;
        }

        [[nodiscard]] constexpr Matrix4 operator-(const Matrix4& o) const noexcept
        {
            Matrix4 r;
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    r.m[i][j] = m[i][j] - o.m[i][j];
            return r;
        }

        [[nodiscard]] constexpr Matrix4 operator*(T s) const noexcept
        {
            Matrix4 r;
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    r.m[i][j] = m[i][j] * s;
            return r;
        }

        [[nodiscard]] constexpr Matrix4 operator-() const noexcept
        {
            Matrix4 r;
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    r.m[i][j] = -m[i][j];
            return r;
        }

        // --- 比較（浮動小数向けの相対＋絶対誤差） ---

        [[nodiscard]] constexpr bool NearlyEqual(const Matrix4& o,
            T abs_eps = T(1e-6),
            T rel_eps = T(1e-5)) const noexcept
        {
            for (int i = 0; i < 4; ++i)
            {
                for (int j = 0; j < 4; ++j)
                {
                    const T a = m[i][j];
                    const T b = o.m[i][j];
                    const T diff = (a > b) ? (a - b) : (b - a);
                    const T scale = std::max<T>(std::abs(a), std::abs(b));
                    if (diff > abs_eps + rel_eps * scale) return false;
                }
            }
            return true;
        }

        [[nodiscard]] constexpr bool operator==(const Matrix4& o) const noexcept
        {
            // 既定は型に応じたepsilon（float/double）を使った近似比較
            return NearlyEqual(o,
                T(10) * std::numeric_limits<T>::epsilon(),      // abs
                T(100) * std::numeric_limits<T>::epsilon());     // rel
        }
        [[nodiscard]] constexpr bool operator!=(const Matrix4& o) const noexcept { return !(*this == o); }

        // --- 逆行列（浮動小数型にのみ提供） ---

        template<class U = T, std::enable_if_t<std::is_floating_point_v<U>, int> = 0>
        void Inverse() noexcept
        {
            const int N = 4;
            U sweep[N][N * 2]{};

            for (int i = 0; i < N; ++i)
            {
                for (int j = 0; j < N; ++j)
                {
                    sweep[i][j] = static_cast<U>(m[i][j]);
                    sweep[i][j + N] = (i == j) ? U{ 1 } : U{ 0 };
                }
            }

            for (int k = 0; k < N; ++k)
            {
                // ピボット選択（部分ピボット）
                U maxv = std::abs(sweep[k][k]); int max_i = k;
                for (int i = k + 1; i < N; ++i)
                {
                    const U v = std::abs(sweep[i][k]);
                    if (v > maxv) { maxv = v; max_i = i; }
                }
                if (maxv <= U(1e-12)) { InitializeIdentity(); return; }

                if (k != max_i)
                    for (int j = 0; j < N * 2; ++j) std::swap(sweep[k][j], sweep[max_i][j]);

                const U pivot = sweep[k][k];
                for (int j = 0; j < N * 2; ++j) sweep[k][j] /= pivot;

                for (int i = 0; i < N; ++i) if (i != k)
                {
                    const U f = sweep[i][k];
                    if (f != U{ 0 })
                        for (int j = 0; j < N * 2; ++j) sweep[i][j] -= sweep[k][j] * f;
                }
            }

            for (int i = 0; i < N; ++i)
                for (int j = 0; j < N; ++j)
                    m[i][j] = static_cast<T>(sweep[i][j + N]);
        }

        template<class U = T, std::enable_if_t<std::is_floating_point_v<U>, int> = 0>
        [[nodiscard]] static Matrix4 Inverse(const Matrix4& a) noexcept
        {
            Matrix4 r = a; r.Inverse(); return r;
        }

        // --- 配列との相互変換（row-major: out[row*4+col]） ---

        constexpr void ToArray16(T out[16]) const noexcept
        {
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    out[i * 4 + j] = m[i][j];
        }

        [[nodiscard]] static constexpr Matrix4 FromArray16(const T in[16]) noexcept
        {
            Matrix4 r;
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    r.m[i][j] = in[i * 4 + j];
            return r;
        }

        // --- 検算：inv が mat の逆行列か（‖mat*inv - I‖∞ を評価） ---
        template<class U = T, std::enable_if_t<std::is_floating_point_v<U>, int> = 0>
        [[nodiscard]] static bool CheckInverse(const Matrix4& mat,
            const Matrix4& inv,
            U tol = U(1e-9)) noexcept
        {
            U max_abs_err = U{ 0 };
            // r = mat * inv
            for (int i = 0; i < 4; ++i)
            {
                for (int j = 0; j < 4; ++j)
                {
                    U s = U{ 0 };
                    for (int k = 0; k < 4; ++k)
                        s += static_cast<U>(mat.m[i][k]) * static_cast<U>(inv.m[k][j]);

                    const U ideal = (i == j) ? U{ 1 } : U{ 0 };
                    const U err = std::abs(ideal - s);
                    if (err > max_abs_err) max_abs_err = err;
                }
            }
            return max_abs_err <= tol;
        }
    };

    using float4x4 = Matrix4<float>;
    using double4x4 = Matrix4<double>;
};

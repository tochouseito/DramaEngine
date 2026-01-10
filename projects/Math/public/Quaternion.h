#pragma once
#include <cmath>
#include <limits>

namespace Drama::Math
{
    /// @brief クォータニオン構造体（w + xi + yj + zk）
    struct Quaternion final
    {
        float m_x;
        float m_y;
        float m_z;
        float m_w;

        /*================ コンストラクタ/初期化 ================*/
        /// @brief デフォルトは単位（0,0,0,1）
        constexpr Quaternion(float x = 0.0f, float y = 0.0f, float z = 0.0f, float w = 1.0f) noexcept
            : m_x(x)
            , m_y(y)
            , m_z(z)
            , m_w(w)
        {
        }

        /// @brief 単位に初期化
        void initialize() noexcept
        {
            // 1) 単位クォータニオンに戻す
            m_x = 0.0f;
            m_y = 0.0f;
            m_z = 0.0f;
            m_w = 1.0f;
        }

        /*================ 四則/積 ================*/
        /// @brief 和
        Quaternion operator+(const Quaternion& other) const noexcept
        {
            // 1) 成分ごとの加算結果を返す
            return { m_x + other.m_x, m_y + other.m_y, m_z + other.m_z, m_w + other.m_w };
        }

        /// @brief 差
        Quaternion operator-(const Quaternion& other) const noexcept
        {
            // 1) 成分ごとの減算結果を返す
            return { m_x - other.m_x, m_y - other.m_y, m_z - other.m_z, m_w - other.m_w };
        }

        /// @brief スカラー乗算
        Quaternion operator*(float scalar) const noexcept
        {
            // 1) 成分ごとにスカラーを掛ける
            return { m_x * scalar, m_y * scalar, m_z * scalar, m_w * scalar };
        }

        /// @brief スカラー除算（0割は単位を返す）
        Quaternion operator/(float scalar) const noexcept
        {
            // 1) 0割は単位クォータニオンでフォールバックする
            if (scalar == 0.0f)
            {
                return identity();
            }
            return { m_x / scalar, m_y / scalar, m_z / scalar, m_w / scalar };
        }

        /// @brief 積（this * other）
        Quaternion multiply(const Quaternion& other) const noexcept
        {
            // 1) ハミルトン積で合成する
            return {
                m_w * other.m_x + m_x * other.m_w + m_y * other.m_z - m_z * other.m_y,
                m_w * other.m_y - m_x * other.m_z + m_y * other.m_w + m_z * other.m_x,
                m_w * other.m_z + m_x * other.m_y - m_y * other.m_x + m_z * other.m_w,
                m_w * other.m_w - m_x * other.m_x - m_y * other.m_y - m_z * other.m_z
            };
        }

        /// @brief 積（演算子）
        Quaternion operator*(const Quaternion& other) const noexcept
        {
            // 1) 積の実装を再利用する
            return multiply(other);
        }

        /*================ 基本演算 ================*/
        /// @brief 共役（その場で反転）
        void conjugate() noexcept
        {
            // 1) ベクトル成分の符号を反転する
            m_x = -m_x;
            m_y = -m_y;
            m_z = -m_z;
            // m_w はそのまま
        }

        /// @brief ノルム
        float norm() const noexcept
        {
            // 1) 成分の二乗和からノルムを計算する
            return std::sqrt(m_x * m_x + m_y * m_y + m_z * m_z + m_w * m_w);
        }

        /// @brief 内積
        float dot(const Quaternion& other) const noexcept
        {
            // 1) 成分ごとの積和を計算する
            return m_x * other.m_x + m_y * other.m_y + m_z * other.m_z + m_w * other.m_w;
        }

        /// @brief 正規化（その場）
        Quaternion normalize() noexcept
        {
            // 1) ノルムで割って単位化する
            const float n = norm();
            if (n == 0.0f)
            {
                initialize();
            }
            else
            {
                m_x /= n;
                m_y /= n;
                m_z /= n;
                m_w /= n;
            }
            return *this;
        }

        /// @brief 逆（その場）
        void inverse() noexcept
        {
            // 1) 共役とノルムから逆クォータニオンを求める
            const Quaternion conj = conjugate_copy(*this);
            const float n = norm();
            const float n2 = n * n;
            if (n2 == 0.0f)
            {
                initialize();
            }
            else
            {
                m_x = conj.m_x / n2;
                m_y = conj.m_y / n2;
                m_z = conj.m_z / n2;
                m_w = conj.m_w / n2;
            }
        }

    public:
        /*================ 静的ユーティリティ ================*/
        /// @brief 線形補間（正規化付き）
        static Quaternion lerp(const Quaternion& start, const Quaternion& end, float t) noexcept
        {
            // 1) 線形補間して正規化する
            Quaternion result = start * (1.0f - t) + end * t;
            return result.normalize();
        }

        /// @brief 球面線形補間（大角・小角に自動対応）
        static Quaternion slerp(const Quaternion& start, const Quaternion& end, float t) noexcept
        {
            // 1) 内積と符号で最短経路を確保する
            float dot_value = start.dot(end);
            Quaternion end_adjusted = end;
            if (dot_value < 0.0f)
            {
                end_adjusted = Quaternion{ -end.m_x, -end.m_y, -end.m_z, -end.m_w };
                dot_value = -dot_value;
            }

            // 2) ほぼ同方向なら線形補間で十分
            const float threshold = 0.9995f;
            if (dot_value > threshold)
            {
                return lerp(start, end_adjusted, t);
            }

            // 3) 角度を使って球面補間する
            const float theta0 = std::acos(dot_value);
            const float theta = theta0 * t;
            const float s0 = std::cos(theta) - dot_value * std::sin(theta) / std::sin(theta0);
            const float s1 = std::sin(theta) / std::sin(theta0);
            Quaternion result{
                s0 * start.m_x + s1 * end_adjusted.m_x,
                s0 * start.m_y + s1 * end_adjusted.m_y,
                s0 * start.m_z + s1 * end_adjusted.m_z,
                s0 * start.m_w + s1 * end_adjusted.m_w
            };
            return result.normalize();
        }

        /// @brief 単位
        static Quaternion identity() noexcept
        {
            // 1) 単位クォータニオンを返す
            return { 0.0f, 0.0f, 0.0f, 1.0f };
        }

        /// @brief 共役（コピー版）
        static Quaternion conjugate_copy(const Quaternion& value) noexcept
        {
            // 1) ベクトル成分だけ反転したコピーを返す
            return { -value.m_x, -value.m_y, -value.m_z, value.m_w };
        }

        /// @brief 正規化（コピー版）
        static Quaternion normalize(const Quaternion& value) noexcept
        {
            // 1) ノルムが 0 のときは単位を返す
            const float n = std::sqrt(value.m_x * value.m_x +
                value.m_y * value.m_y +
                value.m_z * value.m_z +
                value.m_w * value.m_w);
            if (n == 0.0f)
            {
                return identity();
            }
            return { value.m_x / n, value.m_y / n, value.m_z / n, value.m_w / n };
        }

        /// @brief 逆（コピー版）
        static Quaternion inverse(const Quaternion& value) noexcept
        {
            // 1) 共役とノルムから逆クォータニオンを求める
            const Quaternion conj = conjugate_copy(value);
            const float n = std::sqrt(value.m_x * value.m_x +
                value.m_y * value.m_y +
                value.m_z * value.m_z +
                value.m_w * value.m_w);
            const float n2 = n * n;
            if (n2 == 0.0f)
            {
                return identity();
            }
            return { conj.m_x / n2, conj.m_y / n2, conj.m_z / n2, conj.m_w / n2 };
        }

        /*================ Epsilon 比較 ================*/
        /// @brief 既定Epsilonでの等価（|a-b|<=ε を全成分で）
        static bool equals_epsilon(const Quaternion& left, const Quaternion& right) noexcept
        {
            // 1) 既定の許容誤差で比較する
            constexpr float epsilon = 10.0f * std::numeric_limits<float>::epsilon();
            auto abs_value = [](float value)
            {
                return value >= 0.0f ? value : -value;
            };
            return abs_value(left.m_x - right.m_x) <= epsilon &&
                abs_value(left.m_y - right.m_y) <= epsilon &&
                abs_value(left.m_z - right.m_z) <= epsilon &&
                abs_value(left.m_w - right.m_w) <= epsilon;
        }

        /// @brief Epsilon指定の等価
        static bool equals_epsilon(const Quaternion& left, const Quaternion& right, float epsilon) noexcept
        {
            // 1) 指定の許容誤差で比較する
            auto abs_value = [](float value)
            {
                return value >= 0.0f ? value : -value;
            };
            return abs_value(left.m_x - right.m_x) <= epsilon &&
                abs_value(left.m_y - right.m_y) <= epsilon &&
                abs_value(left.m_z - right.m_z) <= epsilon &&
                abs_value(left.m_w - right.m_w) <= epsilon;
        }
    };
} // Drama::Math 名前空間

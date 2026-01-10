#pragma once
#include <cmath>
#include <limits>

namespace Drama::Math
{
    /// @brief クォータニオン構造体（w + xi + yj + zk）
    struct Quaternion final
    {
        float x, y, z, w;

        /*================ コンストラクタ/初期化 ================*/
        /// @brief デフォルトは単位（0,0,0,1）
        constexpr Quaternion(float x_ = 0.0f, float y_ = 0.0f, float z_ = 0.0f, float w_ = 1.0f) : x(x_), y(y_), z(z_), w(w_) {}
        /// @brief 単位に初期化
        void Initialize() { x = 0.0f; y = 0.0f; z = 0.0f; w = 1.0f; }

        /*================ 四則/積 ================*/
        /// @brief 和
        Quaternion operator+(const Quaternion& o) const { return { x + o.x, y + o.y, z + o.z, w + o.w }; }
        /// @brief 差
        Quaternion operator-(const Quaternion& o) const { return { x - o.x, y - o.y, z - o.z, w - o.w }; }
        /// @brief スカラー乗算
        Quaternion operator*(float s) const { return { x * s, y * s, z * s, w * s }; }
        /// @brief スカラー除算（0割は単位を返す）
        Quaternion operator/(float s) const { return (s == 0.0f) ? Identity() : Quaternion{ x / s,y / s,z / s,w / s }; }

        /// @brief 積（this * o）
        Quaternion Multiply(const Quaternion& o) const
        {
            return {
                w * o.x + x * o.w + y * o.z - z * o.y,
                w * o.y - x * o.z + y * o.w + z * o.x,
                w * o.z + x * o.y - y * o.x + z * o.w,
                w * o.w - x * o.x - y * o.y - z * o.z
            };
        }
        /// @brief 積（演算子）
        Quaternion operator*(const Quaternion& o) const { return Multiply(o); }

        /*================ 基本演算 ================*/
        /// @brief 共役（その場で反転）
        void Conjugate() { x = -x; y = -y; z = -z; /* wはそのまま */ }
        /// @brief ノルム
        float Norm() const { return std::sqrt(x * x + y * y + z * z + w * w); }
        /// @brief 内積
        float Dot(const Quaternion& o) const { return x * o.x + y * o.y + z * o.z + w * o.w; }
        /// @brief 正規化（その場）
        Quaternion Normalize()
        {
            float n = Norm();
            if (n == 0.0f) { Initialize(); }
            else { x /= n; y /= n; z /= n; w /= n; }
            return *this;
        }
        /// @brief 逆（その場）
        void Inverse()
        {
            Quaternion c = ConjugateCopy(*this);
            float n = Norm(), ns = n * n;
            if (ns == 0.0f) { Initialize(); }
            else { x = c.x / ns; y = c.y / ns; z = c.z / ns; w = c.w / ns; }
        }

    public: /*================ 静的ユーティリティ ================*/
        /// @brief 線形補間（正規化付き）
        static Quaternion Lerp(const Quaternion& a, const Quaternion& b, float t)
        {
            Quaternion r = a * (1.0f - t) + b * t; return r.Normalize();
        }

        /// @brief 球面線形補間（大角・小角に自動対応）
        static Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t)
        {
            float dot = a.Dot(b);
            Quaternion b2 = (dot < 0.0f) ? Quaternion{ -b.x,-b.y,-b.z,-b.w } : b; // 反転で最短経路
            dot = std::fabs(dot);

            const float th = 0.9995f; // ほぼ同方向ならLerpで十分
            if (dot > th) { return Lerp(a, b2, t); }

            float theta0 = std::acos(dot), theta = theta0 * t;
            float s0 = std::cos(theta) - dot * std::sin(theta) / std::sin(theta0);
            float s1 = std::sin(theta) / std::sin(theta0);
            Quaternion r{ s0 * a.x + s1 * b2.x, s0 * a.y + s1 * b2.y, s0 * a.z + s1 * b2.z, s0 * a.w + s1 * b2.w };
            return r.Normalize();
        }

        /// @brief 単位
        static Quaternion Identity() { return { 0.0f,0.0f,0.0f,1.0f }; }

        /// @brief 共役（コピー版）
        static Quaternion ConjugateCopy(const Quaternion& q) { return { -q.x, -q.y, -q.z, q.w }; }

        /// @brief 正規化（コピー版）
        static Quaternion Normalize(const Quaternion& q)
        {
            float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
            if (n == 0.0f) return Identity();
            return { q.x / n, q.y / n, q.z / n, q.w / n };
        }

        /// @brief 逆（コピー版）
        static Quaternion Inverse(const Quaternion& q)
        {
            Quaternion c = ConjugateCopy(q);
            float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w), ns = n * n;
            return (ns == 0.0f) ? Identity() : Quaternion{ c.x / ns, c.y / ns, c.z / ns, c.w / ns };
        }

        /*================ Epsilon 比較 ================*/
        /// @brief 既定Epsilonでの等価（|a-b|<=ε を全成分で）
        static bool EqualsEpsilon(const Quaternion& a, const Quaternion& b)
        {
            constexpr float e = 10.0f * std::numeric_limits<float>::epsilon();
            auto ab = [](float v) { return v >= 0.0f ? v : -v; };
            return ab(a.x - b.x) <= e && ab(a.y - b.y) <= e && ab(a.z - b.z) <= e && ab(a.w - b.w) <= e;
        }
        /// @brief Epsilon指定の等価
        static bool EqualsEpsilon(const Quaternion& a, const Quaternion& b, float e)
        {
            auto ab = [](float v) { return v >= 0.0f ? v : -v; };
            return ab(a.x - b.x) <= e && ab(a.y - b.y) <= e && ab(a.z - b.z) <= e && ab(a.w - b.w) <= e;
        }
    };
};

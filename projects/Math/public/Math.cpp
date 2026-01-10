#include "pch.h"
#include "Math.h"

namespace Drama::Math
{
    /// @brief 回転行列→クォータニオン（スケール除去＆数値ガード＆正規化付き）
/// 行列 m は row-major で m[row][col] としてアクセス可能な想定。
/// 3x3 部分に回転以外（スケール等）が混ざっていても列正規化である程度補正します。
    Quaternion FromMatrix(const float4x4& m) noexcept
    {
        // 1) 3x3 の取り出し
        float r00 = m.m[0][0], r01 = m.m[0][1], r02 = m.m[0][2];
        float r10 = m.m[1][0], r11 = m.m[1][1], r12 = m.m[1][2];
        float r20 = m.m[2][0], r21 = m.m[2][1], r22 = m.m[2][2];

        // 2) 列ベクトルのスケールを除去（純回転想定を近づける）
        auto safe_inv = [](float a) noexcept {
            constexpr float eps = 1e-12f;
            return (std::fabs(a) < eps) ? 0.0f : (1.0f / a);
            };
        auto len = [](float x, float y, float z) noexcept {
            return std::sqrt(x * x + y * y + z * z);
            };

        // 列0,1,2 の長さ
        float sx = len(r00, r10, r20);
        float sy = len(r01, r11, r21);
        float sz = len(r02, r12, r22);

        // 長さが極小ならその列は触らない（ゼロ除算回避）。通常は正規化。
        if (sx > 0.0f) { float isx = safe_inv(sx); r00 *= isx; r10 *= isx; r20 *= isx; }
        if (sy > 0.0f) { float isy = safe_inv(sy); r01 *= isy; r11 *= isy; r21 *= isy; }
        if (sz > 0.0f) { float isz = safe_inv(sz); r02 *= isz; r12 *= isz; r22 *= isz; }

        // 3) 右手/左手のねじれチェック（任意）：
        //    右手系なら (c0 × c1)·c2 ≈ +1 になるはず。負なら反転で補正。
        {
            // 列ベクトル
            const float c0x = r00, c0y = r10, c0z = r20;
            const float c1x = r01, c1y = r11, c1z = r21;
            const float c2x = r02, c2y = r12, c2z = r22;
            // cross(c0, c1) · c2
            float cx = c0y * c1z - c0z * c1y;
            float cy = c0z * c1x - c0x * c1z;
            float cz = c0x * c1y - c0y * c1x;
            float dot = cx * c2x + cy * c2y + cz * c2z;
            if (dot < 0.0f)
            {
                // 行列の符号を反転（片方の列を反転でもOK）
                r00 = -r00; r10 = -r10; r20 = -r20;
                r01 = -r01; r11 = -r11; r21 = -r21;
                r02 = -r02; r12 = -r12; r22 = -r22;
            }
        }

        // 4) クォータニオン算出（最大対角要素分岐＋数値ガード）
        Quaternion q;
        float trace = r00 + r11 + r22;

        auto q_normalize = [](Quaternion& q) noexcept {
            float n2 = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
            if (n2 > 0.0f)
            {
                float invn = 1.0f / std::sqrt(n2);
                q.x *= invn; q.y *= invn; q.z *= invn; q.w *= invn;
            }
            else
            {
                q = Quaternion{ 0,0,0,1 }; // フォールバック
            }
            };

        constexpr float EPS = 1e-12f;

        if (trace > 0.0f)
        {
            float t = std::max(trace + 1.0f, EPS);
            float s = 2.0f * std::sqrt(t);
            float invs = (s > EPS) ? (1.0f / s) : 0.0f;
            q.w = 0.25f * s;
            q.x = (r21 - r12) * invs;
            q.y = (r02 - r20) * invs;
            q.z = (r10 - r01) * invs;
        }
        else if (r00 > r11 && r00 > r22)
        {
            float t = std::max(1.0f + r00 - r11 - r22, EPS);
            float s = 2.0f * std::sqrt(t);
            float invs = (s > EPS) ? (1.0f / s) : 0.0f;
            q.w = (r21 - r12) * invs;
            q.x = 0.25f * s;
            q.y = (r01 + r10) * invs;
            q.z = (r02 + r20) * invs;
        }
        else if (r11 > r22)
        {
            float t = std::max(1.0f + r11 - r00 - r22, EPS);
            float s = 2.0f * std::sqrt(t);
            float invs = (s > EPS) ? (1.0f / s) : 0.0f;
            q.w = (r02 - r20) * invs;
            q.x = (r01 + r10) * invs;
            q.y = 0.25f * s;
            q.z = (r12 + r21) * invs;
        }
        else
        {
            float t = std::max(1.0f + r22 - r00 - r11, EPS);
            float s = 2.0f * std::sqrt(t);
            float invs = (s > EPS) ? (1.0f / s) : 0.0f;
            q.w = (r10 - r01) * invs;
            q.x = (r02 + r20) * invs;
            q.y = (r12 + r21) * invs;
            q.z = 0.25f * s;
        }

        // 5) 最終正規化（安全）
        q_normalize(q);
        return q;
    }

    [[nodiscard]]
    float3 TransformPoint(const float3& v, const float4x4& M) noexcept
    {
        float3 r;
        r.x = v.x * M.m[0][0] + v.y * M.m[1][0] + v.z * M.m[2][0] + M.m[3][0];
        r.y = v.x * M.m[0][1] + v.y * M.m[1][1] + v.z * M.m[2][1] + M.m[3][1];
        r.z = v.x * M.m[0][2] + v.y * M.m[1][2] + v.z * M.m[2][2] + M.m[3][2];

        const float w = v.x * M.m[0][3] + v.y * M.m[1][3] + v.z * M.m[2][3] + M.m[3][3];
        // 射影を通る場合にのみ意味がある（ワールド/ビューだけなら w≒1）
        constexpr float eps = 1e-8f;
        if (std::fabs(w) > eps)
        {
            const float invW = 1.0f / w;
            r.x *= invW; r.y *= invW; r.z *= invW;
        } // w が極小/0 のときは「未定義」なので、ここでは未除算で返す
        return r;
    }

    [[nodiscard]]
    float3 TransformVector(const float3& v, const float4x4& M) noexcept
    {
        // 方向ベクトル（w=0）→ 平行移動は乗らない・除算もしない
        return {
            v.x * M.m[0][0] + v.y * M.m[1][0] + v.z * M.m[2][0],
            v.x * M.m[0][1] + v.y * M.m[1][1] + v.z * M.m[2][1],
            v.x * M.m[0][2] + v.y * M.m[1][2] + v.z * M.m[2][2]
        };
    }

    [[nodiscard]]
    float4x4 TranslateMatrix(const float3& t) noexcept
    {
        float4x4 m = float4x4::Identity();
        m.m[3][0] = t.x;
        m.m[3][1] = t.y;
        m.m[3][2] = t.z;
        return m;
    }

    [[nodiscard]]
    float4x4 ScaleMatrix(float s) noexcept
    {
        float4x4 m = float4x4::Identity();
        m.m[0][0] = s;
        m.m[1][1] = s;
        m.m[2][2] = s;
        return m;
    }

    [[nodiscard]]
    float4x4 ScaleMatrix(const float3& s) noexcept
    {
        float4x4 m = float4x4::Identity();
        m.m[0][0] = s.x;
        m.m[1][1] = s.y;
        m.m[2][2] = s.z;
        return m;
    }

    [[nodiscard]]
    float4x4 ScaleMatrix(const Scale& s) noexcept
    {
        float4x4 m = float4x4::Identity();
        m.m[0][0] = s.x;
        m.m[1][1] = s.y;
        m.m[2][2] = s.z;
        return m;
    }

    [[nodiscard]]
    float4x4 XAxisMatrix(float radian) noexcept
    {
        float4x4 m = float4x4::Identity();
        const float c = std::cos(radian);
        const float s = std::sin(radian);
        m.m[1][1] = c;  m.m[1][2] = s;
        m.m[2][1] = -s; m.m[2][2] = c;
        return m;
    }

    [[nodiscard]]
    float4x4 YAxisMatrix(float radian) noexcept
    {
        float4x4 m = float4x4::Identity();
        const float c = std::cos(radian);
        const float s = std::sin(radian);
        m.m[0][0] = c;  m.m[0][2] = -s;
        m.m[2][0] = s;  m.m[2][2] = c;
        return m;
    }

    [[nodiscard]]
    float4x4 ZAxisMatrix(float radian) noexcept
    {
        float4x4 m = float4x4::Identity();
        const float c = std::cos(radian);
        const float s = std::sin(radian);
        m.m[0][0] = c;  m.m[0][1] = s;
        m.m[1][0] = -s; m.m[1][1] = c;
        return m;
    }

    [[nodiscard]]
    float4x4 RotateXYZMatrix(const float3& r) noexcept
    {
        return XAxisMatrix(r.x) * YAxisMatrix(r.y) * ZAxisMatrix(r.z);
    }

    [[nodiscard]]
    float4x4 ViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth) noexcept
    {
        float4x4 m = float4x4::Identity();
        m.m[0][0] = width / 2.0f;
        m.m[1][1] = -height / 2.0f; // Y軸反転
        m.m[2][2] = maxDepth - minDepth;
        m.m[3][0] = left + width / 2.0f;
        m.m[3][1] = top + height / 2.0f;
        m.m[3][2] = minDepth;
        return m;
    }

    [[nodiscard]]
    float4x4 PerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip) noexcept
    {
        float4x4 m = float4x4::Zero();
        const float f = std::tan(fovY / 2.0f);
        m.m[0][0] = 1.0f / (aspectRatio * f);
        m.m[1][1] = 1.0f / f;
        m.m[2][2] = (farClip + nearClip) / (farClip - nearClip);
        m.m[2][3] = 1.0f;
        m.m[3][2] = -(2.0f * farClip * nearClip) / (farClip - nearClip);
        return m;
    }

    [[nodiscard]]
    float4x4 OrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) noexcept
    {
        float4x4 m = float4x4::Identity();
        m.m[0][0] = 2.0f / (right - left);
        m.m[1][1] = 2.0f / (top - bottom);
        m.m[2][2] = 1.0f / (farClip - nearClip);
        m.m[3][0] = (left + right) / (left - right);
        m.m[3][1] = (top + bottom) / (bottom - top);
        m.m[3][2] = nearClip / (nearClip - farClip);
        return m;
    }

    [[nodiscard]]
    float Normalize(float x, float min, float max) noexcept
    {
        if (max - min == 0.0f)
        {
            return 0.0f; // ゼロ除算回避
        }
        float n = (x - min) / (max - min);
        return Clamp(n, 0.0f, 1.0f);
    }

    float4x4 MakeRotateAxisAngle(const float3& axis, float angle)
    {
        float3 normAxis = axis;
        float axisLength = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
        if (axisLength != 0.0f)
        {
            normAxis.x /= axisLength;
            normAxis.y /= axisLength;
            normAxis.z /= axisLength;
        }

        float cosTheta = std::cos(angle);
        float sinTheta = std::sin(angle);
        float oneMinusCosTheta = 1.0f - cosTheta;

        float4x4 rotateMatrix;

        rotateMatrix.m[0][0] = cosTheta + normAxis.x * normAxis.x * oneMinusCosTheta;
        rotateMatrix.m[0][1] = normAxis.x * normAxis.y * oneMinusCosTheta - normAxis.z * sinTheta;
        rotateMatrix.m[0][2] = normAxis.x * normAxis.z * oneMinusCosTheta + normAxis.y * sinTheta;
        rotateMatrix.m[0][3] = 0.0f;

        rotateMatrix.m[1][0] = normAxis.y * normAxis.x * oneMinusCosTheta + normAxis.z * sinTheta;
        rotateMatrix.m[1][1] = cosTheta + normAxis.y * normAxis.y * oneMinusCosTheta;
        rotateMatrix.m[1][2] = normAxis.y * normAxis.z * oneMinusCosTheta - normAxis.x * sinTheta;
        rotateMatrix.m[1][3] = 0.0f;

        rotateMatrix.m[2][0] = normAxis.z * normAxis.x * oneMinusCosTheta - normAxis.y * sinTheta;
        rotateMatrix.m[2][1] = normAxis.z * normAxis.y * oneMinusCosTheta + normAxis.x * sinTheta;
        rotateMatrix.m[2][2] = cosTheta + normAxis.z * normAxis.z * oneMinusCosTheta;
        rotateMatrix.m[2][3] = 0.0f;

        rotateMatrix.m[3][0] = 0.0f;
        rotateMatrix.m[3][1] = 0.0f;
        rotateMatrix.m[3][2] = 0.0f;
        rotateMatrix.m[3][3] = 1.0f;

        rotateMatrix = float4x4::Transpose(rotateMatrix);

        return rotateMatrix;
    }

    float4x4 DirectionToDirection(const float3& from, const float3& to)
    {
        // 入力ベクトルを正規化
        float3 normalizedFrom = float3::Normalize(from);
        float3 normalizedTo = float3::Normalize(to);

        // 回転軸を計算
        float3 axis = float3::Cross(normalizedFrom, normalizedTo);
        axis.Normalize();

        // 特殊ケース: from と -to が一致する場合
        if (normalizedFrom == -normalizedTo)
        {
            // 任意の直交軸を選択
            if (std::abs(normalizedFrom.x) < std::abs(normalizedFrom.y))
            {
                axis = float3::Normalize(float3{ 0.0f, -normalizedFrom.z, normalizedFrom.y });
            }
            else
            {
                axis = float3::Normalize(float3{ -normalizedFrom.y, normalizedFrom.x, 0.0f });
            }
        }

        // cosTheta と sinTheta を計算
        float cosTheta = normalizedFrom.Dot(normalizedTo);
        float sinTheta = std::sqrt(1.0f - cosTheta * cosTheta);

        // 最初から転置された形で回転行列を作成
        float4x4 rotateMatrix = {
            (axis.x * axis.x) * (1 - cosTheta) + cosTheta,        (axis.x * axis.y) * (1 - cosTheta) + (axis.z * sinTheta), (axis.x * axis.z) * (1 - cosTheta) - (axis.y * sinTheta), 0.0f,
            (axis.x * axis.y) * (1 - cosTheta) - (axis.z * sinTheta), (axis.y * axis.y) * (1 - cosTheta) + cosTheta,        (axis.y * axis.z) * (1 - cosTheta) + (axis.x * sinTheta), 0.0f,
            (axis.x * axis.z) * (1 - cosTheta) + (axis.y * sinTheta), (axis.y * axis.z) * (1 - cosTheta) - (axis.x * sinTheta), (axis.z * axis.z) * (1 - cosTheta) + cosTheta,        0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };

        return rotateMatrix;
    }

    float DegreesToRadians(float degrees)
    {
        return degrees * PI / 180.0f;
    }

    float3 DegreesToRadians(const float3& degrees)
    {
        return float3(
            degrees.x * PI / 180.0f,
            degrees.y * PI / 180.0f,
            degrees.z * PI / 180.0f
        );
    }

    float RadiansToDegrees(float radians)
    {
        return radians * 180.0f / PI;
    }

    float3 RadiansToDegrees(const float3& radians)
    {
        return float3(
            radians.x * 180.0f / PI,
            radians.y * 180.0f / PI,
            radians.z * 180.0f / PI
        );
    }

    Quaternion MakeRotateAxisAngleQuaternion(const float3& axis, float angle)
    {
        float3 normAxis = axis;

        if (normAxis.Length() == 0.0f)
        {
            return Quaternion(0.0f, 0.0f, 0.0f, 1.0f); // 単位クォータニオンを返す
        }

        normAxis.Normalize();
        float sinHalfAngle = std::sin(angle / 2.0f);
        float cosHalfAngle = std::cos(angle / 2.0f);
        return { normAxis.x * sinHalfAngle, normAxis.y * sinHalfAngle, normAxis.z * sinHalfAngle, cosHalfAngle };
    }

    float3 RotateVector(const float3& vector, const Quaternion& quaternion)
    {
        // q * v * q^-1 を計算する
        Quaternion qv = { vector.x, vector.y, vector.z, 0.0f };

        // クォータニオンの共役を計算
        Quaternion qConjugate = { -quaternion.x, -quaternion.y, -quaternion.z, quaternion.w };

        // q * v
        Quaternion qvRotated = {
            quaternion.w * qv.x + quaternion.y * qv.z - quaternion.z * qv.y,
            quaternion.w * qv.y + quaternion.z * qv.x - quaternion.x * qv.z,
            quaternion.w * qv.z + quaternion.x * qv.y - quaternion.y * qv.x,
            -quaternion.x * qv.x - quaternion.y * qv.y - quaternion.z * qv.z
        };

        // (q * v) * q^-1
        Quaternion result = {
            qvRotated.w * qConjugate.x + qvRotated.x * qConjugate.w + qvRotated.y * qConjugate.z - qvRotated.z * qConjugate.y,
            qvRotated.w * qConjugate.y + qvRotated.y * qConjugate.w + qvRotated.z * qConjugate.x - qvRotated.x * qConjugate.z,
            qvRotated.w * qConjugate.z + qvRotated.z * qConjugate.w + qvRotated.x * qConjugate.y - qvRotated.y * qConjugate.x,
            -qvRotated.x * qConjugate.x - qvRotated.y * qConjugate.y - qvRotated.z * qConjugate.z
        };

        // 結果をベクトルとして返す
        return { result.x, result.y, result.z };
    }

    float4x4 MakeRotateMatrix(const Quaternion& quaternion)
    {
        float4x4 matrix;

        // クォータニオン成分の積
        float xx = quaternion.x * quaternion.x;
        float yy = quaternion.y * quaternion.y;
        float zz = quaternion.z * quaternion.z;
        float xy = quaternion.x * quaternion.y;
        float xz = quaternion.x * quaternion.z;
        float yz = quaternion.y * quaternion.z;
        float wx = quaternion.w * quaternion.x;
        float wy = quaternion.w * quaternion.y;
        float wz = quaternion.w * quaternion.z;

        // 左手座標系用の回転行列
        matrix.m[0][0] = 1.0f - 2.0f * (yy + zz);
        matrix.m[0][1] = 2.0f * (xy + wz); // 符号反転なし
        matrix.m[0][2] = 2.0f * (xz - wy); // 符号反転（Z軸符号反転）
        matrix.m[0][3] = 0.0f;

        matrix.m[1][0] = 2.0f * (xy - wz); // 符号反転なし
        matrix.m[1][1] = 1.0f - 2.0f * (xx + zz);
        matrix.m[1][2] = 2.0f * (yz + wx); // 符号反転（Z軸符号反転）
        matrix.m[1][3] = 0.0f;

        matrix.m[2][0] = 2.0f * (xz + wy); // 符号反転（Z軸符号反転）
        matrix.m[2][1] = 2.0f * (yz - wx); // 符号反転（Z軸符号反転）
        matrix.m[2][2] = 1.0f - 2.0f * (xx + yy);
        matrix.m[2][3] = 0.0f;

        matrix.m[3][0] = 0.0f;
        matrix.m[3][1] = 0.0f;
        matrix.m[3][2] = 0.0f;
        matrix.m[3][3] = 1.0f;

        return matrix;
    }

    Quaternion FromEulerAngles(const float3& eulerAngles)
    {
        Quaternion qx = MakeRotateAxisAngleQuaternion(float3(1.0f, 0.0f, 0.0f), eulerAngles.x);
        Quaternion qy = MakeRotateAxisAngleQuaternion(float3(0.0f, 1.0f, 0.0f), eulerAngles.y);
        Quaternion qz = MakeRotateAxisAngleQuaternion(float3(0.0f, 0.0f, 1.0f), eulerAngles.z);

        // ZYX の順に合成
        return qz * qx * qy;
    }

    float3 ToEulerAngles(const Quaternion& q, RotationOrder order)
    {
        float3 angles;

        switch (order)
        {
        case RotationOrder::XYZ: {
            float sinp = 2 * (q.w * q.x + q.y * q.z);
            if (std::abs(sinp) >= 1)
                angles.x = std::copysign(PI / 2, sinp);
            else
                angles.x = std::asin(sinp);

            float siny_cosp = 2 * (q.w * q.y - q.z * q.x);
            float cosy_cosp = 1 - 2 * (q.x * q.x + q.y * q.y);
            angles.y = std::atan2(siny_cosp, cosy_cosp);

            float sinr_cosp = 2 * (q.w * q.z + q.x * q.y);
            float cosr_cosp = 1 - 2 * (q.y * q.y + q.z * q.z);
            angles.z = std::atan2(sinr_cosp, cosr_cosp);
            break;
        }

        case RotationOrder::YXZ: {
            float sinp = -2 * (q.w * q.y - q.x * q.z);
            if (std::abs(sinp) >= 1)
                angles.x = std::copysign(PI / 2, sinp);
            else
                angles.x = std::asin(sinp);

            float siny_cosp = 2 * (q.w * q.x + q.z * q.y);
            float cosy_cosp = 1 - 2 * (q.y * q.y + q.x * q.x);
            angles.y = std::atan2(siny_cosp, cosy_cosp);

            float sinr_cosp = 2 * (q.w * q.z - q.x * q.y);
            float cosr_cosp = 1 - 2 * (q.z * q.z + q.x * q.x);
            angles.z = std::atan2(sinr_cosp, cosr_cosp);
            break;
        }

        case RotationOrder::ZXY: {
            float sinp = 2 * (q.w * q.z - q.x * q.y);
            if (std::abs(sinp) >= 1)
                angles.x = std::copysign(PI / 2, sinp);
            else
                angles.x = std::asin(sinp);

            float siny_cosp = 2 * (q.w * q.x + q.y * q.z);
            float cosy_cosp = 1 - 2 * (q.z * q.z + q.x * q.x);
            angles.y = std::atan2(siny_cosp, cosy_cosp);

            float sinr_cosp = 2 * (q.w * q.y - q.z * q.x);
            float cosr_cosp = 1 - 2 * (q.y * q.y + q.z * q.z);
            angles.z = std::atan2(sinr_cosp, cosr_cosp);
            break;
        }

        case RotationOrder::ZYX: {
            float sinp = 2 * (q.w * q.y + q.z * q.x);
            if (std::abs(sinp) >= 1)
                angles.x = std::copysign(PI / 2, sinp);
            else
                angles.x = std::asin(sinp);

            float siny_cosp = 2 * (q.w * q.x - q.y * q.z);
            float cosy_cosp = 1 - 2 * (q.z * q.z + q.x * q.x);
            angles.y = std::atan2(siny_cosp, cosy_cosp);

            float sinr_cosp = 2 * (q.w * q.z - q.x * q.y);
            float cosr_cosp = 1 - 2 * (q.y * q.y + q.z * q.z);
            angles.z = std::atan2(sinr_cosp, cosr_cosp);
            break;
        }

        case RotationOrder::YZX: {
            float sinp = 2 * (q.w * q.y - q.x * q.z);
            if (std::abs(sinp) >= 1)
                angles.x = std::copysign(PI / 2, sinp);
            else
                angles.x = std::asin(sinp);

            float siny_cosp = 2 * (q.w * q.z + q.x * q.y);
            float cosy_cosp = 1 - 2 * (q.y * q.y + q.z * q.z);
            angles.y = std::atan2(siny_cosp, cosy_cosp);

            float sinr_cosp = 2 * (q.w * q.x - q.z * q.y);
            float cosr_cosp = 1 - 2 * (q.z * q.z + q.x * q.x);
            angles.z = std::atan2(sinr_cosp, cosr_cosp);
            break;
        }

        case RotationOrder::XZY: {
            float sinp = -2 * (q.w * q.x - q.y * q.z);
            if (std::abs(sinp) >= 1)
                angles.x = std::copysign(PI / 2, sinp);
            else
                angles.x = std::asin(sinp);

            float siny_cosp = 2 * (q.w * q.z + q.x * q.y);
            float cosy_cosp = 1 - 2 * (q.z * q.z + q.x * q.x);
            angles.y = std::atan2(siny_cosp, cosy_cosp);

            float sinr_cosp = 2 * (q.w * q.y - q.x * q.z);
            float cosr_cosp = 1 - 2 * (q.y * q.y + q.z * q.z);
            angles.z = std::atan2(sinr_cosp, cosr_cosp);
            break;
        }

        default:
            break;
        }

        return angles;
    }

    float4x4 MakeAffineMatrix(const float3& scale, const float3& rotate, const float3& translate)
    {
        float4x4 result;
        float4x4 scaleMatrix = ScaleMatrix(scale);
        float4x4 rotateXMatrix = XAxisMatrix(rotate.x);
        float4x4 rotateYMatrix = YAxisMatrix(rotate.y);
        float4x4 rotateZMatrix = ZAxisMatrix(rotate.z);
        float4x4 rotateXYZMatrix = rotateXMatrix * rotateYMatrix * rotateZMatrix;

        float4x4 translateMatrix = TranslateMatrix(translate);
        // result = Multiply(rotateXYZMatrix, Multiply(scaleMatrix, translateMatrix));
        result = scaleMatrix * rotateXYZMatrix * translateMatrix;
        // result = Multiply(scaleMatrix, Multiply(rotateXYZMatrix, translateMatrix));
        // result = Multiply(translateMatrix, Multiply(scaleMatrix, rotateXYZMatrix));
        return result;
    }

    float4x4 MakeAffineMatrix(const Scale& scale, const Quaternion& rotate, const float3& translate)
    {
        float4x4 result;
        float4x4 scaleMatrix = ScaleMatrix(scale);
        float4x4 rotateMatrix = MakeRotateMatrix(rotate);
        float4x4 translateMatrix = TranslateMatrix(translate);
        result = scaleMatrix * rotateMatrix * translateMatrix;
        return result;
    }

    float3 TransformDirection(const float3& v, const float4x4& m)
    {
        return {
            m.m[0][0] * v.x + m.m[1][0] * v.y + m.m[2][0] * v.z,
            m.m[0][1] * v.x + m.m[1][1] * v.y + m.m[2][1] * v.z,
            m.m[0][2] * v.x + m.m[1][2] * v.y + m.m[2][2] * v.z
        };
    }

    float LerpShortAngle(float startAngle, float endAngle, float t)
    {
        // 角度の差を計算
        float delta = std::fmod(endAngle - startAngle, 2 * std::numbers::pi_v<float>);

        // 最短距離を考慮して、必要ならば角度の差を調整
        if (delta > std::numbers::pi_v<float>)
        {
            delta -= 2 * std::numbers::pi_v<float>;
        }
        else if (delta < -std::numbers::pi_v<float>)
        {
            delta += 2 * std::numbers::pi_v<float>;
        }

        // Lerpを使って角度を補間
        float result = startAngle + delta * t;

        // 結果を0~2πの範囲に収める
        result = std::fmod(result, 2 * std::numbers::pi_v<float>);
        if (result < 0.0f)
        {
            result += 2 * std::numbers::pi_v<float>;
        }

        return result;
    }

    Quaternion MakeLookRotation(const float3& forward, const float3& up)
    {
        float3 f = forward;
        f.Normalize();

        float3 r = float3::Cross(up, f); // 右ベクトル = 上 × 前
        r.Normalize();

        float3 u = float3::Cross(f, r); // 上ベクトル = 前 × 右

        // 回転行列を構築（右・上・前）
        float4x4 rotMat = {
            r.x, u.x, f.x, 0.0f,
            r.y, u.y, f.y, 0.0f,
            r.z, u.z, f.z, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };

        return FromMatrix(rotMat); // 回転行列 → クォータニオン変換
    }

    float3 GetForwardVectorFromMatrix(const float4x4& rotMatrix)
    {
        return float3(
            rotMatrix.m[0][2], // x
            rotMatrix.m[1][2], // y
            rotMatrix.m[2][2]  // z
        );
    }

    SRT DecomposeMatrix(const float4x4& in)
    {
        SRT out;

        float4x4 mat = in; // コピーして操作する
        mat.Transpose(); // 行列を転置（左手座標系用）

        // 1) 平行移動成分は m[row][col] の (2,3)
        out.translation.x = mat.m[0][3];
        out.translation.y = mat.m[1][3];
        out.translation.z = mat.m[2][3];

        // 2) スケール成分は各カラムベクトルの長さ
        float3 col0 = { mat.m[0][0], mat.m[1][0], mat.m[2][0] };
        float3 col1 = { mat.m[0][1], mat.m[1][1], mat.m[2][1] };
        float3 col2 = { mat.m[0][2], mat.m[1][2], mat.m[2][2] };
        out.scale.x = std::sqrt(col0.x * col0.x + col0.y * col0.y + col0.z * col0.z);
        out.scale.y = std::sqrt(col1.x * col1.x + col1.y * col1.y + col1.z * col1.z);
        out.scale.z = std::sqrt(col2.x * col2.x + col2.y * col2.y + col2.z * col2.z);

        // 3) 純粋な回転行列を取り出す
        float4x4 rotMat = mat;
        // 各軸ベクトルを正規化
        if (out.scale.x != 0) { rotMat.m[0][0] /= out.scale.x; rotMat.m[1][0] /= out.scale.x; rotMat.m[2][0] /= out.scale.x; }
        if (out.scale.y != 0) { rotMat.m[0][1] /= out.scale.y; rotMat.m[1][1] /= out.scale.y; rotMat.m[2][1] /= out.scale.y; }
        if (out.scale.z != 0) { rotMat.m[0][2] /= out.scale.z; rotMat.m[1][2] /= out.scale.z; rotMat.m[2][2] /= out.scale.z; }

        // 回転行列 → オイラー角（XYZ 順）
        {
            // まず Y 軸回転 β を取得（列0, 行2）
            float sy = rotMat.m[0][2];
            // asin の入力範囲をクリップしておく（数値誤差対策）
            if (sy > 1.0f) sy = 1.0f;
            if (sy < -1.0f) sy = -1.0f;
            out.rotationEuler.y = std::asin(sy);

            // cos(β) でジンバルロック判定
            float cosY = std::cos(out.rotationEuler.y);
            if (std::fabs(cosY) > 1e-6f)
            {
                // β が ±90° でないときは通常分解
                // α (X 軸回転) = atan2( -m[1][2], m[2][2] )
                out.rotationEuler.x = std::atan2(-rotMat.m[1][2], rotMat.m[2][2]);
                // γ (Z 軸回転) = atan2( -m[0][1], m[0][0] )
                out.rotationEuler.z = std::atan2(-rotMat.m[0][1], rotMat.m[0][0]);
            }
            else
            {
                // ジンバルロック時：β = ±90° のとき
                // α を 0 に固定し、γ を別式で算出
                out.rotationEuler.x = 0.0f;
                out.rotationEuler.z = std::atan2(rotMat.m[1][0], rotMat.m[1][1]);
            }
        }

        return out;
    }

    Quaternion MakeQuaternionRotation(const float3& rad, const float3& preRad, const Quaternion& quaternion)
    {
        // 差分計算
        float3 diff = rad - preRad;

        // 各軸のクオータニオンを作成
        Quaternion qx = MakeRotateAxisAngleQuaternion(float3(1.0f, 0.0f, 0.0f), diff.x);
        Quaternion qy = MakeRotateAxisAngleQuaternion(float3(0.0f, 1.0f, 0.0f), diff.y);
        Quaternion qz = MakeRotateAxisAngleQuaternion(float3(0.0f, 0.0f, 1.0f), diff.z);

        // 同時回転を累積
        Quaternion q = quaternion * qx * qy * qz;
        return q.Normalize(); // 正規化して返す
    }

    Quaternion MakeEulerRotation(const float3& rad)
    {
        // オイラー角からクォータニオンを作成
        // 各軸のクオータニオンを作成
        Quaternion qx = MakeRotateAxisAngleQuaternion(float3(1.0f, 0.0f, 0.0f), rad.x);
        Quaternion qy = MakeRotateAxisAngleQuaternion(float3(0.0f, 1.0f, 0.0f), rad.y);
        Quaternion qz = MakeRotateAxisAngleQuaternion(float3(0.0f, 0.0f, 1.0f), rad.z);

        // 同時回転を累積
        Quaternion q = qx * qy * qz;
        return q.Normalize(); // 正規化して返す
    }

    float4x4 BillboardMatrix(const float4x4 cameraMatrix)
    {
        float4x4 result;

        float cosY = cos(PI);
        float sinY = sin(PI);

        float4x4 backToFrontMatrix = {
            cosY, 0.0f, -sinY, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            sinY, 0.0f, cosY, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f };

        result = backToFrontMatrix * cameraMatrix;
        result.m[3][0] = 0.0f;
        result.m[3][1] = 0.0f;
        result.m[3][2] = 0.0f;

        return result;
    }

    float2 WorldToScreen(const float3& worldPos, const float4x4& viewMatrix, const float4x4& projMatrix, uint32_t screenWidth, uint32_t screenHeight)
    {
        // Viewport行列
        float4x4 viewportMatrix = ViewportMatrix(0.0f, 0.0f, static_cast<float>(screenWidth), static_cast<float>(screenHeight), 0.0f, 1.0f);
        // View * Projection * Viewport
        float4x4 vpMatrix = projMatrix * viewMatrix * viewportMatrix;
        // 変換
        float3 screenPos = TransformPoint(worldPos, vpMatrix);
        return float2(screenPos.x, screenPos.y);
    }

    float Lerp(float start, float end, float t)
    {
        return start + (end - start) * t;
    }

    // 移動成分のみ抽出
    float3 GetTranslation(const float4x4& m)
    {
        return float3(m.m[3][0], m.m[3][1], m.m[3][2]);
    }

    // 球面線形補間 (Slerp) 関数
    float3 Slerp(const float3& v1, const float3& v2, float t)
    {
        // 正規化
        float3 start = v1;
        float3 end = v2;
        float dot = start.x * end.x + start.y * end.y + start.z * end.z;
        if (dot < 0.0f)
        {
            end.x = -end.x;
            end.y = -end.y;
            end.z = -end.z;
            dot = -dot;
        }

        const float threshold = 0.9995f;
        if (dot > threshold)
        {
            // 線形補間
            return float3{
                v1.x + t * (v2.x - v1.x), v1.y + t * (v2.y - v1.y), v1.z + t * (v2.z - v1.z) };
        }

        // 角度を計算
        float theta = std::acos(dot);
        float invSinTheta = 1.0f / std::sin(theta);

        // 球面線形補間
        float scale1 = std::sin((1.0f - t) * theta) * invSinTheta;
        float scale2 = std::sin(t * theta) * invSinTheta;

        return float3{
            scale1 * v1.x + scale2 * v2.x,
            scale1 * v1.y + scale2 * v2.y,
            scale1 * v1.z + scale2 * v2.z
        };
    }

    // 線形（普通のLerpと同じ）
    // 線形（普通のLerpと同じ）
    float easing::Linear(float t)
    {
        return t;
    }

    // 二乗で加速（最初ゆっくり → 後半速い）
    // 二乗で加速（最初ゆっくり → 後半速い）
    float easing::EaseInQuad(float t)
    {
        return t * t;
    }

    // 二乗で減速（最初速い → 後半ゆっくり）
    // 二乗で減速（最初速い → 後半ゆっくり）
    float easing::EaseOutQuad(float t)
    {
        return 1.0f - (1.0f - t) * (1.0f - t);
    }

    // 二乗で加減速（最初ゆっくり → 中盤速い → 最後ゆっくり）
    // 二乗で加減速（最初ゆっくり → 中盤速い → 最後ゆっくり）
    float easing::EaseInOutQuad(float t)
    {
        return t < 0.5f ? 2.0f * t * t : 1.0f - powf(-2.0f * t + 2.0f, 2) / 2.0f;
    }
} // namespace Drama::Math

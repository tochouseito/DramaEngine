#include "pch.h"
#include "Math.h"

namespace Drama::Math
{
    /// @brief 回転行列→クォータニオン（スケール除去＆数値ガード＆正規化付き）
    /// 行列 m は row-major で m[row][col] としてアクセス可能な想定。
    /// 3x3 部分に回転以外（スケール等）が混ざっていても列正規化である程度補正します。
    Quaternion from_matrix(const Float4x4& m) noexcept
    {
        // 1) 3x3 の取り出し
        float r00 = m.m_values[0][0], r01 = m.m_values[0][1], r02 = m.m_values[0][2];
        float r10 = m.m_values[1][0], r11 = m.m_values[1][1], r12 = m.m_values[1][2];
        float r20 = m.m_values[2][0], r21 = m.m_values[2][1], r22 = m.m_values[2][2];

        // 2) 列ベクトルのスケールを除去（純回転想定を近づける）
        auto safeInv = [](float value) noexcept
        {
            constexpr float eps = 1e-12f;
            return (std::fabs(value) < eps) ? 0.0f : (1.0f / value);
        };
        auto length = [](float x, float y, float z) noexcept
        {
            return std::sqrt(x * x + y * y + z * z);
        };

        // 3) 列0,1,2 の長さ
        float sx = length(r00, r10, r20);
        float sy = length(r01, r11, r21);
        float sz = length(r02, r12, r22);

        // 4) 長さが極小ならその列は触らない（ゼロ除算回避）。通常は正規化。
        if (sx > 0.0f)
        {
            const float invSx = safeInv(sx);
            r00 *= invSx;
            r10 *= invSx;
            r20 *= invSx;
        }
        if (sy > 0.0f)
        {
            const float invSy = safeInv(sy);
            r01 *= invSy;
            r11 *= invSy;
            r21 *= invSy;
        }
        if (sz > 0.0f)
        {
            const float invSz = safeInv(sz);
            r02 *= invSz;
            r12 *= invSz;
            r22 *= invSz;
        }

        // 5) 右手/左手のねじれチェック（任意）
        {
            const float c0x = r00, c0y = r10, c0z = r20;
            const float c1x = r01, c1y = r11, c1z = r21;
            const float c2x = r02, c2y = r12, c2z = r22;
            float cx = c0y * c1z - c0z * c1y;
            float cy = c0z * c1x - c0x * c1z;
            float cz = c0x * c1y - c0y * c1x;
            float dot = cx * c2x + cy * c2y + cz * c2z;
            if (dot < 0.0f)
            {
                // 行列の符号を反転（片方の列を反転でもOK）
                r00 = -r00;
                r10 = -r10;
                r20 = -r20;
                r01 = -r01;
                r11 = -r11;
                r21 = -r21;
                r02 = -r02;
                r12 = -r12;
                r22 = -r22;
            }
        }

        // 6) クォータニオン算出（最大対角要素分岐＋数値ガード）
        Quaternion q;
        float trace = r00 + r11 + r22;

        auto normalizeQuaternion = [](Quaternion& target) noexcept
        {
            float n2 = target.m_x * target.m_x + target.m_y * target.m_y + target.m_z * target.m_z + target.m_w * target.m_w;
            if (n2 > 0.0f)
            {
                float invn = 1.0f / std::sqrt(n2);
                target.m_x *= invn;
                target.m_y *= invn;
                target.m_z *= invn;
                target.m_w *= invn;
            }
            else
            {
                target = Quaternion{ 0.0f, 0.0f, 0.0f, 1.0f }; // フォールバック
            }
        };

        constexpr float epsilon = 1e-12f;

        if (trace > 0.0f)
        {
            float t = std::max(trace + 1.0f, epsilon);
            float s = 2.0f * std::sqrt(t);
            float invs = (s > epsilon) ? (1.0f / s) : 0.0f;
            q.m_w = 0.25f * s;
            q.m_x = (r21 - r12) * invs;
            q.m_y = (r02 - r20) * invs;
            q.m_z = (r10 - r01) * invs;
        }
        else if (r00 > r11 && r00 > r22)
        {
            float t = std::max(1.0f + r00 - r11 - r22, epsilon);
            float s = 2.0f * std::sqrt(t);
            float invs = (s > epsilon) ? (1.0f / s) : 0.0f;
            q.m_w = (r21 - r12) * invs;
            q.m_x = 0.25f * s;
            q.m_y = (r01 + r10) * invs;
            q.m_z = (r02 + r20) * invs;
        }
        else if (r11 > r22)
        {
            float t = std::max(1.0f + r11 - r00 - r22, epsilon);
            float s = 2.0f * std::sqrt(t);
            float invs = (s > epsilon) ? (1.0f / s) : 0.0f;
            q.m_w = (r02 - r20) * invs;
            q.m_x = (r01 + r10) * invs;
            q.m_y = 0.25f * s;
            q.m_z = (r12 + r21) * invs;
        }
        else
        {
            float t = std::max(1.0f + r22 - r00 - r11, epsilon);
            float s = 2.0f * std::sqrt(t);
            float invs = (s > epsilon) ? (1.0f / s) : 0.0f;
            q.m_w = (r10 - r01) * invs;
            q.m_x = (r02 + r20) * invs;
            q.m_y = (r12 + r21) * invs;
            q.m_z = 0.25f * s;
        }

        // 7) 最終正規化（安全）
        normalizeQuaternion(q);
        return q;
    }
    [[nodiscard]]
    Float3 transform_point(const Float3& v, const Float4x4& matrix) noexcept
    {
        // 1) 平行移動込みの座標変換を行う
        Float3 result;
        result.m_x = v.m_x * matrix.m_values[0][0] + v.m_y * matrix.m_values[1][0] + v.m_z * matrix.m_values[2][0] + matrix.m_values[3][0];
        result.m_y = v.m_x * matrix.m_values[0][1] + v.m_y * matrix.m_values[1][1] + v.m_z * matrix.m_values[2][1] + matrix.m_values[3][1];
        result.m_z = v.m_x * matrix.m_values[0][2] + v.m_y * matrix.m_values[1][2] + v.m_z * matrix.m_values[2][2] + matrix.m_values[3][2];

        // 2) 射影除算が必要な場合のみ適用する
        const float w = v.m_x * matrix.m_values[0][3] + v.m_y * matrix.m_values[1][3] + v.m_z * matrix.m_values[2][3] + matrix.m_values[3][3];
        constexpr float eps = 1e-8f;
        if (std::fabs(w) > eps)
        {
            const float invW = 1.0f / w;
            result.m_x *= invW;
            result.m_y *= invW;
            result.m_z *= invW;
        }
        return result;
    }

    [[nodiscard]]
    Float3 transform_vector(const Float3& v, const Float4x4& matrix) noexcept
    {
        // 1) 方向ベクトルとして回転・スケール成分のみ適用する
        return {
            v.m_x * matrix.m_values[0][0] + v.m_y * matrix.m_values[1][0] + v.m_z * matrix.m_values[2][0],
            v.m_x * matrix.m_values[0][1] + v.m_y * matrix.m_values[1][1] + v.m_z * matrix.m_values[2][1],
            v.m_x * matrix.m_values[0][2] + v.m_y * matrix.m_values[1][2] + v.m_z * matrix.m_values[2][2]
        };
    }

    [[nodiscard]]
    Float4x4 translate_matrix(const Float3& t) noexcept
    {
        // 1) 単位行列に平行移動成分を設定する
        Float4x4 matrix = Float4x4::identity();
        matrix.m_values[3][0] = t.m_x;
        matrix.m_values[3][1] = t.m_y;
        matrix.m_values[3][2] = t.m_z;
        return matrix;
    }

    [[nodiscard]]
    Float4x4 scale_matrix(float s) noexcept
    {
        // 1) 単位行列の対角成分にスケールを設定する
        Float4x4 matrix = Float4x4::identity();
        matrix.m_values[0][0] = s;
        matrix.m_values[1][1] = s;
        matrix.m_values[2][2] = s;
        return matrix;
    }

    [[nodiscard]]
    Float4x4 scale_matrix(const Float3& s) noexcept
    {
        // 1) 各軸のスケールを対角成分に設定する
        Float4x4 matrix = Float4x4::identity();
        matrix.m_values[0][0] = s.m_x;
        matrix.m_values[1][1] = s.m_y;
        matrix.m_values[2][2] = s.m_z;
        return matrix;
    }

    [[nodiscard]]
    Float4x4 scale_matrix(const Scale& s) noexcept
    {
        // 1) スケール構造体の値を対角成分に設定する
        Float4x4 matrix = Float4x4::identity();
        matrix.m_values[0][0] = s.m_x;
        matrix.m_values[1][1] = s.m_y;
        matrix.m_values[2][2] = s.m_z;
        return matrix;
    }

    [[nodiscard]]
    Float4x4 x_axis_matrix(float radian) noexcept
    {
        // 1) X軸回転行列を構築する
        Float4x4 matrix = Float4x4::identity();
        const float c = std::cos(radian);
        const float s = std::sin(radian);
        matrix.m_values[1][1] = c;
        matrix.m_values[1][2] = s;
        matrix.m_values[2][1] = -s;
        matrix.m_values[2][2] = c;
        return matrix;
    }

    [[nodiscard]]
    Float4x4 y_axis_matrix(float radian) noexcept
    {
        // 1) Y軸回転行列を構築する
        Float4x4 matrix = Float4x4::identity();
        const float c = std::cos(radian);
        const float s = std::sin(radian);
        matrix.m_values[0][0] = c;
        matrix.m_values[0][2] = -s;
        matrix.m_values[2][0] = s;
        matrix.m_values[2][2] = c;
        return matrix;
    }

    [[nodiscard]]
    Float4x4 z_axis_matrix(float radian) noexcept
    {
        // 1) Z軸回転行列を構築する
        Float4x4 matrix = Float4x4::identity();
        const float c = std::cos(radian);
        const float s = std::sin(radian);
        matrix.m_values[0][0] = c;
        matrix.m_values[0][1] = s;
        matrix.m_values[1][0] = -s;
        matrix.m_values[1][1] = c;
        return matrix;
    }

    [[nodiscard]]
    Float4x4 rotate_xyz_matrix(const Float3& r) noexcept
    {
        // 1) 各軸回転を合成する
        return x_axis_matrix(r.m_x) * y_axis_matrix(r.m_y) * z_axis_matrix(r.m_z);
    }

    [[nodiscard]]
    Float4x4 viewport_matrix(float left, float top, float width, float height, float minDepth, float maxDepth) noexcept
    {
        // 1) 画面座標への変換行列を構築する
        Float4x4 matrix = Float4x4::identity();
        matrix.m_values[0][0] = width / 2.0f;
        matrix.m_values[1][1] = -height / 2.0f; // Y軸反転
        matrix.m_values[2][2] = maxDepth - minDepth;
        matrix.m_values[3][0] = left + width / 2.0f;
        matrix.m_values[3][1] = top + height / 2.0f;
        matrix.m_values[3][2] = minDepth;
        return matrix;
    }

    [[nodiscard]]
    Float4x4 perspective_fov_matrix(float fovY, float aspectRatio, float nearClip, float farClip) noexcept
    {
        // 1) 透視投影行列を構築する
        Float4x4 matrix = Float4x4::zero();
        const float f = std::tan(fovY / 2.0f);
        matrix.m_values[0][0] = 1.0f / (aspectRatio * f);
        matrix.m_values[1][1] = 1.0f / f;
        matrix.m_values[2][2] = (farClip + nearClip) / (farClip - nearClip);
        matrix.m_values[2][3] = 1.0f;
        matrix.m_values[3][2] = -(2.0f * farClip * nearClip) / (farClip - nearClip);
        return matrix;
    }

    [[nodiscard]]
    Float4x4 orthographic_matrix(float left, float top, float right, float bottom, float nearClip, float farClip) noexcept
    {
        // 1) 正射影行列を構築する
        Float4x4 matrix = Float4x4::identity();
        matrix.m_values[0][0] = 2.0f / (right - left);
        matrix.m_values[1][1] = 2.0f / (top - bottom);
        matrix.m_values[2][2] = 1.0f / (farClip - nearClip);
        matrix.m_values[3][0] = (left + right) / (left - right);
        matrix.m_values[3][1] = (top + bottom) / (bottom - top);
        matrix.m_values[3][2] = nearClip / (nearClip - farClip);
        return matrix;
    }

    [[nodiscard]]
    float normalize(float x, float minValue, float maxValue) noexcept
    {
        // 1) 範囲がゼロの場合は安全値を返す
        if (maxValue - minValue == 0.0f)
        {
            return 0.0f;
        }

        // 2) 0〜1 に正規化してクランプする
        float n = (x - minValue) / (maxValue - minValue);
        return clamp(n, 0.0f, 1.0f);
    }

    Float4x4 make_rotate_axis_angle(const Float3& axis, float angle)
    {
        // 1) 軸を正規化して回転計算に備える
        Float3 normAxis = axis;
        float axisLength = std::sqrt(axis.m_x * axis.m_x + axis.m_y * axis.m_y + axis.m_z * axis.m_z);
        if (axisLength != 0.0f)
        {
            normAxis.m_x /= axisLength;
            normAxis.m_y /= axisLength;
            normAxis.m_z /= axisLength;
        }

        // 2) 回転角の三角関数値を求める
        float cosTheta = std::cos(angle);
        float sinTheta = std::sin(angle);
        float oneMinusCosTheta = 1.0f - cosTheta;

        // 3) 軸回転行列を構築する
        Float4x4 rotateMatrix;

        rotateMatrix.m_values[0][0] = cosTheta + normAxis.m_x * normAxis.m_x * oneMinusCosTheta;
        rotateMatrix.m_values[0][1] = normAxis.m_x * normAxis.m_y * oneMinusCosTheta - normAxis.m_z * sinTheta;
        rotateMatrix.m_values[0][2] = normAxis.m_x * normAxis.m_z * oneMinusCosTheta + normAxis.m_y * sinTheta;
        rotateMatrix.m_values[0][3] = 0.0f;

        rotateMatrix.m_values[1][0] = normAxis.m_y * normAxis.m_x * oneMinusCosTheta + normAxis.m_z * sinTheta;
        rotateMatrix.m_values[1][1] = cosTheta + normAxis.m_y * normAxis.m_y * oneMinusCosTheta;
        rotateMatrix.m_values[1][2] = normAxis.m_y * normAxis.m_z * oneMinusCosTheta - normAxis.m_x * sinTheta;
        rotateMatrix.m_values[1][3] = 0.0f;

        rotateMatrix.m_values[2][0] = normAxis.m_z * normAxis.m_x * oneMinusCosTheta - normAxis.m_y * sinTheta;
        rotateMatrix.m_values[2][1] = normAxis.m_z * normAxis.m_y * oneMinusCosTheta + normAxis.m_x * sinTheta;
        rotateMatrix.m_values[2][2] = cosTheta + normAxis.m_z * normAxis.m_z * oneMinusCosTheta;
        rotateMatrix.m_values[2][3] = 0.0f;

        rotateMatrix.m_values[3][0] = 0.0f;
        rotateMatrix.m_values[3][1] = 0.0f;
        rotateMatrix.m_values[3][2] = 0.0f;
        rotateMatrix.m_values[3][3] = 1.0f;

        rotateMatrix = Float4x4::transpose(rotateMatrix);

        return rotateMatrix;
    }

    Float4x4 direction_to_direction(const Float3& from, const Float3& to)
    {
        // 1) 入力ベクトルを正規化して比較する
        Float3 normalizedFrom = Float3::normalize(from);
        Float3 normalizedTo = Float3::normalize(to);

        // 2) 回転軸を計算する
        Float3 axis = Float3::cross(normalizedFrom, normalizedTo);
        axis.normalize();

        // 3) 特殊ケース: from と -to が一致する場合は直交軸を選ぶ
        if (normalizedFrom == -normalizedTo)
        {
            if (std::abs(normalizedFrom.m_x) < std::abs(normalizedFrom.m_y))
            {
                axis = Float3::normalize(Float3{ 0.0f, -normalizedFrom.m_z, normalizedFrom.m_y });
            }
            else
            {
                axis = Float3::normalize(Float3{ -normalizedFrom.m_y, normalizedFrom.m_x, 0.0f });
            }
        }

        // 4) cosTheta と sinTheta を計算する
        float cosTheta = normalizedFrom.dot(normalizedTo);
        float sinTheta = std::sqrt(1.0f - cosTheta * cosTheta);

        // 5) 最初から転置された形で回転行列を作成する
        Float4x4 rotateMatrix = {
            (axis.m_x * axis.m_x) * (1 - cosTheta) + cosTheta,        (axis.m_x * axis.m_y) * (1 - cosTheta) + (axis.m_z * sinTheta), (axis.m_x * axis.m_z) * (1 - cosTheta) - (axis.m_y * sinTheta), 0.0f,
            (axis.m_x * axis.m_y) * (1 - cosTheta) - (axis.m_z * sinTheta), (axis.m_y * axis.m_y) * (1 - cosTheta) + cosTheta,        (axis.m_y * axis.m_z) * (1 - cosTheta) + (axis.m_x * sinTheta), 0.0f,
            (axis.m_x * axis.m_z) * (1 - cosTheta) + (axis.m_y * sinTheta), (axis.m_y * axis.m_z) * (1 - cosTheta) - (axis.m_x * sinTheta), (axis.m_z * axis.m_z) * (1 - cosTheta) + cosTheta,        0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };

        return rotateMatrix;
    }

    float degrees_to_radians(float degrees)
    {
        // 1) 度数法をラジアンへ変換する
        return degrees * pi / 180.0f;
    }

    Float3 degrees_to_radians(const Float3& degrees)
    {
        // 1) 各成分をラジアンへ変換する
        return Float3(
            degrees.m_x * pi / 180.0f,
            degrees.m_y * pi / 180.0f,
            degrees.m_z * pi / 180.0f
        );
    }

    float radians_to_degrees(float radians)
    {
        // 1) ラジアンを度数法へ変換する
        return radians * 180.0f / pi;
    }

    Float3 radians_to_degrees(const Float3& radians)
    {
        // 1) 各成分を度数法へ変換する
        return Float3(
            radians.m_x * 180.0f / pi,
            radians.m_y * 180.0f / pi,
            radians.m_z * 180.0f / pi
        );
    }

    Quaternion make_rotate_axis_angle_quaternion(const Float3& axis, float angle)
    {
        // 1) 回転軸を正規化して安全に計算する
        Float3 normAxis = axis;

        if (normAxis.length() == 0.0f)
        {
            return Quaternion(0.0f, 0.0f, 0.0f, 1.0f); // 単位クォータニオンを返す
        }

        // 2) 半角で回転量を構成する
        normAxis.normalize();
        float sinHalfAngle = std::sin(angle / 2.0f);
        float cosHalfAngle = std::cos(angle / 2.0f);
        return { normAxis.m_x * sinHalfAngle, normAxis.m_y * sinHalfAngle, normAxis.m_z * sinHalfAngle, cosHalfAngle };
    }

    Float3 rotate_vector(const Float3& vector, const Quaternion& quaternion)
    {
        // 1) q * v * q^-1 を計算する
        Quaternion qv = { vector.m_x, vector.m_y, vector.m_z, 0.0f };

        // 2) クォータニオンの共役を計算する
        Quaternion qConjugate = { -quaternion.m_x, -quaternion.m_y, -quaternion.m_z, quaternion.m_w };

        // 3) q * v を計算する
        Quaternion qvRotated = {
            quaternion.m_w * qv.m_x + quaternion.m_y * qv.m_z - quaternion.m_z * qv.m_y,
            quaternion.m_w * qv.m_y + quaternion.m_z * qv.m_x - quaternion.m_x * qv.m_z,
            quaternion.m_w * qv.m_z + quaternion.m_x * qv.m_y - quaternion.m_y * qv.m_x,
            -quaternion.m_x * qv.m_x - quaternion.m_y * qv.m_y - quaternion.m_z * qv.m_z
        };

        // 4) (q * v) * q^-1 を計算する
        Quaternion result = {
            qvRotated.m_w * qConjugate.m_x + qvRotated.m_x * qConjugate.m_w + qvRotated.m_y * qConjugate.m_z - qvRotated.m_z * qConjugate.m_y,
            qvRotated.m_w * qConjugate.m_y + qvRotated.m_y * qConjugate.m_w + qvRotated.m_z * qConjugate.m_x - qvRotated.m_x * qConjugate.m_z,
            qvRotated.m_w * qConjugate.m_z + qvRotated.m_z * qConjugate.m_w + qvRotated.m_x * qConjugate.m_y - qvRotated.m_y * qConjugate.m_x,
            -qvRotated.m_x * qConjugate.m_x - qvRotated.m_y * qConjugate.m_y - qvRotated.m_z * qConjugate.m_z
        };

        // 5) 結果をベクトルとして返す
        return { result.m_x, result.m_y, result.m_z };
    }

    Float4x4 make_rotate_matrix(const Quaternion& quaternion)
    {
        // 1) クォータニオン成分から回転行列を構築する
        Float4x4 matrix;

        // クォータニオン成分の積
        float xx = quaternion.m_x * quaternion.m_x;
        float yy = quaternion.m_y * quaternion.m_y;
        float zz = quaternion.m_z * quaternion.m_z;
        float xy = quaternion.m_x * quaternion.m_y;
        float xz = quaternion.m_x * quaternion.m_z;
        float yz = quaternion.m_y * quaternion.m_z;
        float wx = quaternion.m_w * quaternion.m_x;
        float wy = quaternion.m_w * quaternion.m_y;
        float wz = quaternion.m_w * quaternion.m_z;

        // 左手座標系用の回転行列
        matrix.m_values[0][0] = 1.0f - 2.0f * (yy + zz);
        matrix.m_values[0][1] = 2.0f * (xy + wz); // 符号反転なし
        matrix.m_values[0][2] = 2.0f * (xz - wy); // 符号反転（Z軸符号反転）
        matrix.m_values[0][3] = 0.0f;

        matrix.m_values[1][0] = 2.0f * (xy - wz); // 符号反転なし
        matrix.m_values[1][1] = 1.0f - 2.0f * (xx + zz);
        matrix.m_values[1][2] = 2.0f * (yz + wx); // 符号反転（Z軸符号反転）
        matrix.m_values[1][3] = 0.0f;

        matrix.m_values[2][0] = 2.0f * (xz + wy); // 符号反転（Z軸符号反転）
        matrix.m_values[2][1] = 2.0f * (yz - wx); // 符号反転（Z軸符号反転）
        matrix.m_values[2][2] = 1.0f - 2.0f * (xx + yy);
        matrix.m_values[2][3] = 0.0f;

        matrix.m_values[3][0] = 0.0f;
        matrix.m_values[3][1] = 0.0f;
        matrix.m_values[3][2] = 0.0f;
        matrix.m_values[3][3] = 1.0f;

        return matrix;
    }

    Quaternion from_euler_angles(const Float3& eulerAngles)
    {
        // 1) 各軸回転のクォータニオンを構成する
        Quaternion qx = make_rotate_axis_angle_quaternion(Float3(1.0f, 0.0f, 0.0f), eulerAngles.m_x);
        Quaternion qy = make_rotate_axis_angle_quaternion(Float3(0.0f, 1.0f, 0.0f), eulerAngles.m_y);
        Quaternion qz = make_rotate_axis_angle_quaternion(Float3(0.0f, 0.0f, 1.0f), eulerAngles.m_z);

        // 2) ZYX の順に合成する
        return qz * qx * qy;
    }

    Float3 to_euler_angles(const Quaternion& q, RotationOrder order)
    {
        // 1) 回転順序に合わせてオイラー角を算出する
        Float3 angles;

        switch (order)
        {
        case RotationOrder::Xyz:
        {
            float sinp = 2 * (q.m_w * q.m_x + q.m_y * q.m_z);
            if (std::abs(sinp) >= 1)
            {
                angles.m_x = std::copysign(pi / 2, sinp);
            }
            else
            {
                angles.m_x = std::asin(sinp);
            }

            float siny_cosp = 2 * (q.m_w * q.m_y - q.m_z * q.m_x);
            float cosy_cosp = 1 - 2 * (q.m_x * q.m_x + q.m_y * q.m_y);
            angles.m_y = std::atan2(siny_cosp, cosy_cosp);

            float sinr_cosp = 2 * (q.m_w * q.m_z + q.m_x * q.m_y);
            float cosr_cosp = 1 - 2 * (q.m_y * q.m_y + q.m_z * q.m_z);
            angles.m_z = std::atan2(sinr_cosp, cosr_cosp);
            break;
        }
        case RotationOrder::Yxz:
        {
            float sinp = -2 * (q.m_w * q.m_y - q.m_x * q.m_z);
            if (std::abs(sinp) >= 1)
            {
                angles.m_x = std::copysign(pi / 2, sinp);
            }
            else
            {
                angles.m_x = std::asin(sinp);
            }

            float siny_cosp = 2 * (q.m_w * q.m_x + q.m_z * q.m_y);
            float cosy_cosp = 1 - 2 * (q.m_y * q.m_y + q.m_x * q.m_x);
            angles.m_y = std::atan2(siny_cosp, cosy_cosp);

            float sinr_cosp = 2 * (q.m_w * q.m_z - q.m_x * q.m_y);
            float cosr_cosp = 1 - 2 * (q.m_z * q.m_z + q.m_x * q.m_x);
            angles.m_z = std::atan2(sinr_cosp, cosr_cosp);
            break;
        }
        case RotationOrder::Zxy:
        {
            float sinp = 2 * (q.m_w * q.m_z - q.m_x * q.m_y);
            if (std::abs(sinp) >= 1)
            {
                angles.m_x = std::copysign(pi / 2, sinp);
            }
            else
            {
                angles.m_x = std::asin(sinp);
            }

            float siny_cosp = 2 * (q.m_w * q.m_x + q.m_y * q.m_z);
            float cosy_cosp = 1 - 2 * (q.m_z * q.m_z + q.m_x * q.m_x);
            angles.m_y = std::atan2(siny_cosp, cosy_cosp);

            float sinr_cosp = 2 * (q.m_w * q.m_y - q.m_z * q.m_x);
            float cosr_cosp = 1 - 2 * (q.m_y * q.m_y + q.m_z * q.m_z);
            angles.m_z = std::atan2(sinr_cosp, cosr_cosp);
            break;
        }
        case RotationOrder::Zyx:
        {
            float sinp = 2 * (q.m_w * q.m_y + q.m_z * q.m_x);
            if (std::abs(sinp) >= 1)
            {
                angles.m_x = std::copysign(pi / 2, sinp);
            }
            else
            {
                angles.m_x = std::asin(sinp);
            }

            float siny_cosp = 2 * (q.m_w * q.m_x - q.m_y * q.m_z);
            float cosy_cosp = 1 - 2 * (q.m_z * q.m_z + q.m_x * q.m_x);
            angles.m_y = std::atan2(siny_cosp, cosy_cosp);

            float sinr_cosp = 2 * (q.m_w * q.m_z - q.m_x * q.m_y);
            float cosr_cosp = 1 - 2 * (q.m_y * q.m_y + q.m_z * q.m_z);
            angles.m_z = std::atan2(sinr_cosp, cosr_cosp);
            break;
        }
        case RotationOrder::Yzx:
        {
            float sinp = 2 * (q.m_w * q.m_y - q.m_x * q.m_z);
            if (std::abs(sinp) >= 1)
            {
                angles.m_x = std::copysign(pi / 2, sinp);
            }
            else
            {
                angles.m_x = std::asin(sinp);
            }

            float siny_cosp = 2 * (q.m_w * q.m_z + q.m_x * q.m_y);
            float cosy_cosp = 1 - 2 * (q.m_y * q.m_y + q.m_z * q.m_z);
            angles.m_y = std::atan2(siny_cosp, cosy_cosp);

            float sinr_cosp = 2 * (q.m_w * q.m_x - q.m_z * q.m_y);
            float cosr_cosp = 1 - 2 * (q.m_z * q.m_z + q.m_x * q.m_x);
            angles.m_z = std::atan2(sinr_cosp, cosr_cosp);
            break;
        }
        case RotationOrder::Xzy:
        {
            float sinp = -2 * (q.m_w * q.m_x - q.m_y * q.m_z);
            if (std::abs(sinp) >= 1)
            {
                angles.m_x = std::copysign(pi / 2, sinp);
            }
            else
            {
                angles.m_x = std::asin(sinp);
            }

            float siny_cosp = 2 * (q.m_w * q.m_z + q.m_x * q.m_y);
            float cosy_cosp = 1 - 2 * (q.m_z * q.m_z + q.m_x * q.m_x);
            angles.m_y = std::atan2(siny_cosp, cosy_cosp);

            float sinr_cosp = 2 * (q.m_w * q.m_y - q.m_x * q.m_z);
            float cosr_cosp = 1 - 2 * (q.m_y * q.m_y + q.m_z * q.m_z);
            angles.m_z = std::atan2(sinr_cosp, cosr_cosp);
            break;
        }
        default:
        {
            break;
        }
        }

        // 2) 算出結果を返す
        return angles;
    }

    Float4x4 make_affine_matrix(const Float3& scale, const Float3& rotate, const Float3& translate)
    {
        // 1) スケール・回転・平行移動を合成する
        Float4x4 result;
        Float4x4 scaleMatrix = scale_matrix(scale);
        Float4x4 rotateXMatrix = x_axis_matrix(rotate.m_x);
        Float4x4 rotateYMatrix = y_axis_matrix(rotate.m_y);
        Float4x4 rotateZMatrix = z_axis_matrix(rotate.m_z);
        Float4x4 rotateXyzMatrix = rotateXMatrix * rotateYMatrix * rotateZMatrix;

        Float4x4 translateMatrix = translate_matrix(translate);
        result = scaleMatrix * rotateXyzMatrix * translateMatrix;
        return result;
    }

    Float4x4 make_affine_matrix(const Scale& scale, const Quaternion& rotate, const Float3& translate)
    {
        // 1) スケール・回転・平行移動を合成する
        Float4x4 result;
        Float4x4 scaleMatrix = scale_matrix(scale);
        Float4x4 rotateMatrix = make_rotate_matrix(rotate);
        Float4x4 translateMatrix = translate_matrix(translate);
        result = scaleMatrix * rotateMatrix * translateMatrix;
        return result;
    }

    Float3 transform_direction(const Float3& v, const Float4x4& matrix)
    {
        // 1) 方向ベクトルとして回転成分のみ適用する
        return {
            matrix.m_values[0][0] * v.m_x + matrix.m_values[1][0] * v.m_y + matrix.m_values[2][0] * v.m_z,
            matrix.m_values[0][1] * v.m_x + matrix.m_values[1][1] * v.m_y + matrix.m_values[2][1] * v.m_z,
            matrix.m_values[0][2] * v.m_x + matrix.m_values[1][2] * v.m_y + matrix.m_values[2][2] * v.m_z
        };
    }

    float lerp_short_angle(float startAngle, float endAngle, float t)
    {
        // 1) 角度差を求めて最短方向へ寄せる
        float delta = std::fmod(endAngle - startAngle, 2 * std::numbers::pi_v<float>);

        if (delta > std::numbers::pi_v<float>)
        {
            delta -= 2 * std::numbers::pi_v<float>;
        }
        else if (delta < -std::numbers::pi_v<float>)
        {
            delta += 2 * std::numbers::pi_v<float>;
        }

        // 2) 補間して角度を求める
        float result = startAngle + delta * t;

        // 3) 結果を 0〜2π に収める
        result = std::fmod(result, 2 * std::numbers::pi_v<float>);
        if (result < 0.0f)
        {
            result += 2 * std::numbers::pi_v<float>;
        }

        return result;
    }

    Quaternion make_look_rotation(const Float3& forward, const Float3& up)
    {
        // 1) 前方向を正規化する
        Float3 f = forward;
        f.normalize();

        // 2) 直交基底を構築する
        Float3 r = Float3::cross(up, f); // 右ベクトル = 上 × 前
        r.normalize();

        Float3 u = Float3::cross(f, r); // 上ベクトル = 前 × 右

        // 3) 回転行列を構築してクォータニオンへ変換する
        Float4x4 rotMat = {
            r.m_x, u.m_x, f.m_x, 0.0f,
            r.m_y, u.m_y, f.m_y, 0.0f,
            r.m_z, u.m_z, f.m_z, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };

        return from_matrix(rotMat);
    }

    Float3 get_forward_vector_from_matrix(const Float4x4& rotMatrix)
    {
        // 1) 回転行列の前方向成分を取り出す
        return Float3(
            rotMatrix.m_values[0][2], // x
            rotMatrix.m_values[1][2], // y
            rotMatrix.m_values[2][2]  // z
        );
    }    Srt decompose_matrix(const Float4x4& in)
    {
        // 1) 入力をコピーして解析用に保持する
        Srt result;
        Float4x4 mat = in;
        mat.transpose();

        // 2) 平行移動成分を抽出する
        result.m_translation.m_x = mat.m_values[0][3];
        result.m_translation.m_y = mat.m_values[1][3];
        result.m_translation.m_z = mat.m_values[2][3];

        // 3) スケール成分を各列ベクトルの長さから求める
        Float3 col0 = { mat.m_values[0][0], mat.m_values[1][0], mat.m_values[2][0] };
        Float3 col1 = { mat.m_values[0][1], mat.m_values[1][1], mat.m_values[2][1] };
        Float3 col2 = { mat.m_values[0][2], mat.m_values[1][2], mat.m_values[2][2] };
        result.m_scale.m_x = std::sqrt(col0.m_x * col0.m_x + col0.m_y * col0.m_y + col0.m_z * col0.m_z);
        result.m_scale.m_y = std::sqrt(col1.m_x * col1.m_x + col1.m_y * col1.m_y + col1.m_z * col1.m_z);
        result.m_scale.m_z = std::sqrt(col2.m_x * col2.m_x + col2.m_y * col2.m_y + col2.m_z * col2.m_z);

        // 4) 純粋な回転行列を取り出す
        Float4x4 rotMat = mat;
        if (result.m_scale.m_x != 0.0f)
        {
            rotMat.m_values[0][0] /= result.m_scale.m_x;
            rotMat.m_values[1][0] /= result.m_scale.m_x;
            rotMat.m_values[2][0] /= result.m_scale.m_x;
        }
        if (result.m_scale.m_y != 0.0f)
        {
            rotMat.m_values[0][1] /= result.m_scale.m_y;
            rotMat.m_values[1][1] /= result.m_scale.m_y;
            rotMat.m_values[2][1] /= result.m_scale.m_y;
        }
        if (result.m_scale.m_z != 0.0f)
        {
            rotMat.m_values[0][2] /= result.m_scale.m_z;
            rotMat.m_values[1][2] /= result.m_scale.m_z;
            rotMat.m_values[2][2] /= result.m_scale.m_z;
        }

        // 5) 回転行列からオイラー角（XYZ 順）を算出する
        float sy = rotMat.m_values[0][2];
        if (sy > 1.0f)
        {
            sy = 1.0f;
        }
        if (sy < -1.0f)
        {
            sy = -1.0f;
        }
        result.m_rotationEuler.m_y = std::asin(sy);

        float cosY = std::cos(result.m_rotationEuler.m_y);
        if (std::fabs(cosY) > 1e-6f)
        {
            result.m_rotationEuler.m_x = std::atan2(-rotMat.m_values[1][2], rotMat.m_values[2][2]);
            result.m_rotationEuler.m_z = std::atan2(-rotMat.m_values[0][1], rotMat.m_values[0][0]);
        }
        else
        {
            result.m_rotationEuler.m_x = 0.0f;
            result.m_rotationEuler.m_z = std::atan2(rotMat.m_values[1][0], rotMat.m_values[1][1]);
        }

        return result;
    }
    Quaternion make_quaternion_rotation(const Float3& rad, const Float3& preRad, const Quaternion& quaternion)
    {
        // 1) 差分角を算出する
        Float3 diff = rad - preRad;

        // 2) 各軸のクォータニオンを作成する
        Quaternion qx = make_rotate_axis_angle_quaternion(Float3(1.0f, 0.0f, 0.0f), diff.m_x);
        Quaternion qy = make_rotate_axis_angle_quaternion(Float3(0.0f, 1.0f, 0.0f), diff.m_y);
        Quaternion qz = make_rotate_axis_angle_quaternion(Float3(0.0f, 0.0f, 1.0f), diff.m_z);

        // 3) 同時回転を累積して正規化する
        Quaternion q = quaternion * qx * qy * qz;
        return q.normalize();
    }

    Quaternion make_euler_rotation(const Float3& rad)
    {
        // 1) 各軸のクォータニオンを作成する
        Quaternion qx = make_rotate_axis_angle_quaternion(Float3(1.0f, 0.0f, 0.0f), rad.m_x);
        Quaternion qy = make_rotate_axis_angle_quaternion(Float3(0.0f, 1.0f, 0.0f), rad.m_y);
        Quaternion qz = make_rotate_axis_angle_quaternion(Float3(0.0f, 0.0f, 1.0f), rad.m_z);

        // 2) 同時回転を累積して正規化する
        Quaternion q = qx * qy * qz;
        return q.normalize();
    }

    Float4x4 billboard_matrix(const Float4x4 cameraMatrix)
    {
        // 1) 正面向きへ回転させる行列を構築する
        Float4x4 result;
        float cosY = std::cos(pi);
        float sinY = std::sin(pi);

        Float4x4 backToFrontMatrix = {
            cosY, 0.0f, -sinY, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            sinY, 0.0f, cosY, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };

        // 2) 回転して平行移動成分を消す
        result = backToFrontMatrix * cameraMatrix;
        result.m_values[3][0] = 0.0f;
        result.m_values[3][1] = 0.0f;
        result.m_values[3][2] = 0.0f;

        return result;
    }

    Float2 world_to_screen(const Float3& worldPos, const Float4x4& viewMatrix, const Float4x4& projMatrix,
        std::uint32_t screenWidth, std::uint32_t screenHeight)
    {
        // 1) Viewport 行列を構築する
        Float4x4 viewportMatrix = viewport_matrix(0.0f, 0.0f, static_cast<float>(screenWidth),
            static_cast<float>(screenHeight), 0.0f, 1.0f);
        // 2) 行列を合成してスクリーン座標へ変換する
        Float4x4 vpMatrix = projMatrix * viewMatrix * viewportMatrix;
        Float3 screenPos = transform_point(worldPos, vpMatrix);
        return Float2(screenPos.m_x, screenPos.m_y);
    }

    float lerp(float start, float end, float t)
    {
        // 1) 線形補間値を返す
        return start + (end - start) * t;
    }

    Float3 get_translation(const Float4x4& matrix)
    {
        // 1) 平行移動成分のみ抽出する
        return Float3(matrix.m_values[3][0], matrix.m_values[3][1], matrix.m_values[3][2]);
    }

    Float3 slerp(const Float3& v1, const Float3& v2, float t)
    {
        // 1) 方向を揃えたうえで内積を求める
        Float3 start = v1;
        Float3 end = v2;
        float dot = start.m_x * end.m_x + start.m_y * end.m_y + start.m_z * end.m_z;
        if (dot < 0.0f)
        {
            end.m_x = -end.m_x;
            end.m_y = -end.m_y;
            end.m_z = -end.m_z;
            dot = -dot;
        }

        const float threshold = 0.9995f;
        if (dot > threshold)
        {
            // 2) ほぼ同方向なら線形補間に落とす
            return Float3{
                v1.m_x + t * (v2.m_x - v1.m_x),
                v1.m_y + t * (v2.m_y - v1.m_y),
                v1.m_z + t * (v2.m_z - v1.m_z)
            };
        }

        // 3) 角度に基づいて球面補間する
        float theta = std::acos(dot);
        float invSinTheta = 1.0f / std::sin(theta);
        float scale1 = std::sin((1.0f - t) * theta) * invSinTheta;
        float scale2 = std::sin(t * theta) * invSinTheta;

        return Float3{
            scale1 * v1.m_x + scale2 * v2.m_x,
            scale1 * v1.m_y + scale2 * v2.m_y,
            scale1 * v1.m_z + scale2 * v2.m_z
        };
    }

    float easing::linear(float t)
    {
        // 1) 線形に補間する
        return t;
    }

    float easing::ease_in_quad(float t)
    {
        // 1) 二乗で加速する
        return t * t;
    }

    float easing::ease_out_quad(float t)
    {
        // 1) 二乗で減速する
        return 1.0f - (1.0f - t) * (1.0f - t);
    }

    float easing::ease_in_out_quad(float t)
    {
        // 1) 二乗で加減速する
        return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2) / 2.0f;
    }
} // namespace Drama::Math




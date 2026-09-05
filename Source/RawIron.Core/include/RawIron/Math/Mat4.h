#pragma once

#include "RawIron/Math/Vec3.h"

#include <cmath>

namespace ri::math {

struct Mat4 {
    float m[4][4]{};
};

inline constexpr float kPi = 3.14159265358979323846f;

inline constexpr float DegreesToRadians(float degrees) {
    return degrees * (kPi / 180.0f);
}

inline constexpr Mat4 IdentityMatrix() {
    return Mat4{{
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    }};
}

inline Mat4 Multiply(const Mat4& lhs, const Mat4& rhs) {
    Mat4 result{};
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            float value = 0.0f;
            for (int index = 0; index < 4; ++index) {
                value += lhs.m[row][index] * rhs.m[index][column];
            }
            result.m[row][column] = value;
        }
    }
    return result;
}

inline Mat4 TranslationMatrix(const Vec3& translation) {
    Mat4 result = IdentityMatrix();
    result.m[0][3] = translation.x;
    result.m[1][3] = translation.y;
    result.m[2][3] = translation.z;
    return result;
}

inline Mat4 ScaleMatrix(const Vec3& scale) {
    Mat4 result = IdentityMatrix();
    result.m[0][0] = scale.x;
    result.m[1][1] = scale.y;
    result.m[2][2] = scale.z;
    return result;
}

inline Mat4 RotationXDegrees(float degrees) {
    const float radians = DegreesToRadians(degrees);
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);

    Mat4 result = IdentityMatrix();
    result.m[1][1] = cosine;
    result.m[1][2] = -sine;
    result.m[2][1] = sine;
    result.m[2][2] = cosine;
    return result;
}

inline Mat4 RotationYDegrees(float degrees) {
    const float radians = DegreesToRadians(degrees);
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);

    Mat4 result = IdentityMatrix();
    result.m[0][0] = cosine;
    result.m[0][2] = sine;
    result.m[2][0] = -sine;
    result.m[2][2] = cosine;
    return result;
}

inline Mat4 RotationZDegrees(float degrees) {
    const float radians = DegreesToRadians(degrees);
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);

    Mat4 result = IdentityMatrix();
    result.m[0][0] = cosine;
    result.m[0][1] = -sine;
    result.m[1][0] = sine;
    result.m[1][1] = cosine;
    return result;
}

inline Mat4 RotationXYZDegrees(const Vec3& rotationDegrees) {
    return Multiply(
        RotationZDegrees(rotationDegrees.z),
        Multiply(RotationYDegrees(rotationDegrees.y), RotationXDegrees(rotationDegrees.x)));
}

inline Mat4 TRS(const Vec3& translation, const Vec3& rotationDegrees, const Vec3& scale) {
    return Multiply(TranslationMatrix(translation), Multiply(RotationXYZDegrees(rotationDegrees), ScaleMatrix(scale)));
}

inline Vec3 ExtractTranslation(const Mat4& matrix) {
    return Vec3{matrix.m[0][3], matrix.m[1][3], matrix.m[2][3]};
}

inline Vec3 ExtractScale(const Mat4& matrix) {
    return Vec3{
        Length(Vec3{matrix.m[0][0], matrix.m[1][0], matrix.m[2][0]}),
        Length(Vec3{matrix.m[0][1], matrix.m[1][1], matrix.m[2][1]}),
        Length(Vec3{matrix.m[0][2], matrix.m[1][2], matrix.m[2][2]}),
    };
}

inline Vec3 TransformPoint(const Mat4& matrix, const Vec3& point) {
    return Vec3{
        (matrix.m[0][0] * point.x) + (matrix.m[0][1] * point.y) + (matrix.m[0][2] * point.z) + matrix.m[0][3],
        (matrix.m[1][0] * point.x) + (matrix.m[1][1] * point.y) + (matrix.m[1][2] * point.z) + matrix.m[1][3],
        (matrix.m[2][0] * point.x) + (matrix.m[2][1] * point.y) + (matrix.m[2][2] * point.z) + matrix.m[2][3],
    };
}

inline Vec3 TransformVector(const Mat4& matrix, const Vec3& vector) {
    return Vec3{
        (matrix.m[0][0] * vector.x) + (matrix.m[0][1] * vector.y) + (matrix.m[0][2] * vector.z),
        (matrix.m[1][0] * vector.x) + (matrix.m[1][1] * vector.y) + (matrix.m[1][2] * vector.z),
        (matrix.m[2][0] * vector.x) + (matrix.m[2][1] * vector.y) + (matrix.m[2][2] * vector.z),
    };
}

inline Vec3 ExtractRight(const Mat4& matrix) {
    return Normalize(Vec3{matrix.m[0][0], matrix.m[1][0], matrix.m[2][0]});
}

inline Vec3 ExtractUp(const Mat4& matrix) {
    return Normalize(Vec3{matrix.m[0][1], matrix.m[1][1], matrix.m[2][1]});
}

inline Vec3 ExtractForward(const Mat4& matrix) {
    return Normalize(Vec3{matrix.m[0][2], matrix.m[1][2], matrix.m[2][2]});
}

inline double LinearDeterminant(const Mat4& matrix) {
    const auto& a = matrix.m;
    return double(a[0][0]) * (double(a[1][1])*a[2][2] - double(a[1][2])*a[2][1])
         - double(a[0][1]) * (double(a[1][0])*a[2][2] - double(a[1][2])*a[2][0])
         + double(a[0][2]) * (double(a[1][0])*a[2][1] - double(a[1][1])*a[2][0]);
}

/// Precomputed inverse-transpose direction transform for affine surface normals.
/// Translation is ignored. Double intermediates preserve tiny/large finite scales;
/// determinant magnitude cancels during normalization, but its sign must remain.
/// Singular/non-finite transforms or normals produce zero for caller face fallback.
class NormalTransform {
public:
    explicit NormalTransform(const Mat4& matrix) {
        for (int row=0; row<3; ++row) for (int col=0; col<3; ++col)
            if (!std::isfinite(matrix.m[row][col])) return;
        const double determinant = LinearDeterminant(matrix);
        if (!std::isfinite(determinant) || determinant == 0.0) return;
        const double sign = determinant < 0 ? -1.0 : 1.0;
        for (int row=0; row<3; ++row) for (int col=0; col<3; ++col) {
            const int r1=(row+1)%3, r2=(row+2)%3, c1=(col+1)%3, c2=(col+2)%3;
            cofactor_[row][col] = sign * (double(matrix.m[r1][c1])*matrix.m[r2][c2]
                                       - double(matrix.m[r1][c2])*matrix.m[r2][c1]);
        }
        valid_ = true;
    }

    [[nodiscard]] Vec3 Apply(const Vec3& normal) const {
        if (!valid_ || !std::isfinite(normal.x) || !std::isfinite(normal.y) || !std::isfinite(normal.z)) return {};
        const double x=cofactor_[0][0]*normal.x+cofactor_[0][1]*normal.y+cofactor_[0][2]*normal.z;
        const double y=cofactor_[1][0]*normal.x+cofactor_[1][1]*normal.y+cofactor_[1][2]*normal.z;
        const double z=cofactor_[2][0]*normal.x+cofactor_[2][1]*normal.y+cofactor_[2][2]*normal.z;
        const double length=std::hypot(x,y,z);
        if (!std::isfinite(length) || length == 0.0) return {};
        return {static_cast<float>(x/length),static_cast<float>(y/length),static_cast<float>(z/length)};
    }
private:
    double cofactor_[3][3]{};
    bool valid_ = false;
};

[[nodiscard]] inline Vec3 TransformNormal(const Mat4& matrix, const Vec3& normal) {
    return NormalTransform(matrix).Apply(normal);
}

/// Builds a world transform whose local XY plane follows the camera orientation.
/// Translation and non-uniform scale come from `authoredWorld`; its authored rotation is replaced.
/// `yawOnly` keeps world-up fixed for vegetation-style cylindrical billboards.
inline Mat4 CameraFacingMatrix(const Mat4& authoredWorld, const Mat4& cameraWorld, bool yawOnly = false) {
    const Vec3 position = ExtractTranslation(authoredWorld);
    const Vec3 scale = ExtractScale(authoredWorld);

    Vec3 right = ExtractRight(cameraWorld);
    Vec3 up = ExtractUp(cameraWorld);
    Vec3 forward = ExtractForward(cameraWorld);
    if (yawOnly) {
        forward.y = 0.0f;
        if (LengthSquared(forward) <= 1.0e-12f) {
            forward = Vec3{0.0f, 0.0f, 1.0f};
        } else {
            forward = Normalize(forward);
        }
        up = Vec3{0.0f, 1.0f, 0.0f};
        right = Normalize(Cross(up, forward));
        if (LengthSquared(right) <= 1.0e-12f) {
            right = Vec3{1.0f, 0.0f, 0.0f};
        }
    }

    Mat4 out = IdentityMatrix();
    out.m[0][0] = right.x * scale.x;
    out.m[1][0] = right.y * scale.x;
    out.m[2][0] = right.z * scale.x;
    out.m[0][1] = up.x * scale.y;
    out.m[1][1] = up.y * scale.y;
    out.m[2][1] = up.z * scale.y;
    out.m[0][2] = forward.x * scale.z;
    out.m[1][2] = forward.y * scale.z;
    out.m[2][2] = forward.z * scale.z;
    out.m[0][3] = position.x;
    out.m[1][3] = position.y;
    out.m[2][3] = position.z;
    return out;
}

/// Inverts an affine transform whose last row is `[0 0 0 1]` (typical scene node world matrices).
/// Returns false if the linear 3×3 part is singular.
[[nodiscard]] inline bool TryInvertAffineMat4(const Mat4& m, Mat4& out) {
    const float a = m.m[0][0];
    const float b = m.m[0][1];
    const float c = m.m[0][2];
    const float d = m.m[1][0];
    const float e = m.m[1][1];
    const float f = m.m[1][2];
    const float g = m.m[2][0];
    const float h = m.m[2][1];
    const float i = m.m[2][2];
    const float tx = m.m[0][3];
    const float ty = m.m[1][3];
    const float tz = m.m[2][3];

    const float det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    if (std::abs(det) < 1.0e-24f) {
        return false;
    }
    const float s = 1.0f / det;

    const float inv00 = (e * i - f * h) * s;
    const float inv01 = (c * h - b * i) * s;
    const float inv02 = (b * f - c * e) * s;
    const float inv10 = (f * g - d * i) * s;
    const float inv11 = (a * i - c * g) * s;
    const float inv12 = (c * d - a * f) * s;
    const float inv20 = (d * h - e * g) * s;
    const float inv21 = (b * g - a * h) * s;
    const float inv22 = (a * e - b * d) * s;

    out.m[0][0] = inv00;
    out.m[0][1] = inv01;
    out.m[0][2] = inv02;
    out.m[1][0] = inv10;
    out.m[1][1] = inv11;
    out.m[1][2] = inv12;
    out.m[2][0] = inv20;
    out.m[2][1] = inv21;
    out.m[2][2] = inv22;

    out.m[0][3] = -(inv00 * tx + inv01 * ty + inv02 * tz);
    out.m[1][3] = -(inv10 * tx + inv11 * ty + inv12 * tz);
    out.m[2][3] = -(inv20 * tx + inv21 * ty + inv22 * tz);

    out.m[3][0] = 0.0f;
    out.m[3][1] = 0.0f;
    out.m[3][2] = 0.0f;
    out.m[3][3] = 1.0f;
    return true;
}

} // namespace ri::math

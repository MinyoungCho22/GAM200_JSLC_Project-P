#pragma once

namespace Math
{
    struct ivec2
    {
        int x, y;

        ivec2();
        ivec2(int x, int y);

        ivec2& operator+=(const ivec2& rhs);
        ivec2& operator-=(const ivec2& rhs);
        ivec2& operator*=(int scalar);
        ivec2& operator/=(int scalar);
    };

    ivec2 operator+(const ivec2& lhs, const ivec2& rhs);
    ivec2 operator-(const ivec2& lhs, const ivec2& rhs);
    ivec2 operator*(const ivec2& lhs, int scalar);
    ivec2 operator*(int scalar, const ivec2& rhs);
    ivec2 operator/(const ivec2& lhs, int scalar);

    struct Vec2
    {
        float x, y;

        Vec2();
        Vec2(float x, float y);

        float Length() const;
        float LengthSq() const;

        Vec2& Normalize();
        Vec2 GetNormalized() const;
        Vec2 Perpendicular() const;

        Vec2& operator+=(const Vec2& rhs);
        Vec2& operator-=(const Vec2& rhs);
        Vec2& operator*=(float scalar);
        Vec2& operator/=(float scalar);
    };

    Vec2 operator+(const Vec2& lhs, const Vec2& rhs);
    Vec2 operator-(const Vec2& lhs, const Vec2& rhs);
    Vec2 operator*(const Vec2& lhs, float scalar);
    Vec2 operator*(float scalar, const Vec2& rhs);
    Vec2 operator/(const Vec2& lhs, float scalar);

    Vec2 Lerp(Vec2 a, Vec2 b, float t);

} // namespace Math
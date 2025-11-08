#pragma once

namespace Math
{
    // 정수 기반 2D 벡터
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

    // ivec2를 위한 비-멤버 연산자
    ivec2 operator+(const ivec2& lhs, const ivec2& rhs);
    ivec2 operator-(const ivec2& lhs, const ivec2& rhs);
    ivec2 operator*(const ivec2& lhs, int scalar);
    ivec2 operator*(int scalar, const ivec2& rhs);
    ivec2 operator/(const ivec2& lhs, int scalar);


    // 실수 기반 2D 벡터
    struct Vec2
    {
        float x, y;

        Vec2();
        Vec2(float x, float y);

        float Length() const;
        float LengthSq() const;

        Vec2& Normalize();
        Vec2 GetNormalized() const;

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

    // Lerp 함수 선언
    // a와 b 벡터 사이를 t(0.0 ~ 1.0) 비율로 선형 보간
    Vec2 Lerp(Vec2 a, Vec2 b, float t);


} // namespace Math
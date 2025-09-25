#pragma once

struct Vec2
{
    float x, y;

    // 생성자
    Vec2();
    Vec2(float x, float y);

    // 벡터의 길이를 계산하는 함수
    float Length() const;
    float LengthSq() const; // 제곱 길이는 루트 계산이 없어 더 빠릅니다.

    // 벡터를 정규화하는 함수 (길이를 1로 만듦)
    Vec2& Normalize();
    Vec2 GetNormalized() const;

    // 기본 연산자 오버로딩
    Vec2& operator+=(const Vec2& rhs);
    Vec2& operator-=(const Vec2& rhs);
    Vec2& operator*=(float scalar);
    Vec2& operator/=(float scalar);
};

// 멤버 함수가 아닌 일반 함수로 연산자 오버로딩 (Vec2 + Vec2 형태)
Vec2 operator+(const Vec2& lhs, const Vec2& rhs);
Vec2 operator-(const Vec2& lhs, const Vec2& rhs);
Vec2 operator*(const Vec2& lhs, float scalar);
Vec2 operator*(float scalar, const Vec2& rhs);
Vec2 operator/(const Vec2& lhs, float scalar);
#include "Vec2.h"
#include <cmath>

Vec2::Vec2() : x(0.0f), y(0.0f) {}
Vec2::Vec2(float x, float y) : x(x), y(y) {}

float Vec2::Length() const { return std::sqrt(x * x + y * y); }
float Vec2::LengthSq() const { return x * x + y * y; }

Vec2& Vec2::Normalize()
{
    const float length = Length();
    if (length != 0.0f) { x /= length; y /= length; }
    return *this;
}

Vec2 Vec2::GetNormalized() const
{
    const float length = Length();
    if (length != 0.0f) { return Vec2(x / length, y / length); }
    return Vec2();
}

Vec2& Vec2::operator+=(const Vec2& rhs) { x += rhs.x; y += rhs.y; return *this; }
Vec2& Vec2::operator-=(const Vec2& rhs) { x -= rhs.x; y -= rhs.y; return *this; }
Vec2& Vec2::operator*=(float scalar) { x *= scalar; y *= scalar; return *this; }
Vec2& Vec2::operator/=(float scalar) { x /= scalar; y /= scalar; return *this; }

Vec2 operator+(const Vec2& lhs, const Vec2& rhs) { return Vec2(lhs.x + rhs.x, lhs.y + rhs.y); }
Vec2 operator-(const Vec2& lhs, const Vec2& rhs) { return Vec2(lhs.x - rhs.x, lhs.y - rhs.y); }
Vec2 operator*(const Vec2& lhs, float scalar) { return Vec2(lhs.x * scalar, lhs.y * scalar); }
Vec2 operator*(float scalar, const Vec2& rhs) { return Vec2(rhs.x * scalar, rhs.y * scalar); }
Vec2 operator/(const Vec2& lhs, float scalar) { return Vec2(lhs.x / scalar, lhs.y / scalar); }
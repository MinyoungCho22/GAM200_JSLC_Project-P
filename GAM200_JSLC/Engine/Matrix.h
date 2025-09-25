#pragma once
#include "Vec2.h"

class Matrix
{
public:
    Matrix();

    Matrix operator*(const Matrix& rhs) const;
    Matrix& operator*=(const Matrix& rhs);

    static Matrix CreateTranslation(const Vec2& offset);
    static Matrix CreateScale(const Vec2& scale);
    static Matrix CreateOrtho(float left, float right, float bottom, float top, float near, float far);

    const float* GetAsFloatPtr() const;

private:
    float m[16];
};
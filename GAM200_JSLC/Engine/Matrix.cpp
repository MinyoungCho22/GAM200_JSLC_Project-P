#include "Matrix.h"
#include <cstring>

Matrix::Matrix()
{
    memset(m, 0, sizeof(float) * 16);
    m[0] = 1.0f;
    m[5] = 1.0f;
    m[10] = 1.0f;
    m[15] = 1.0f;
}

Matrix Matrix::operator*(const Matrix& rhs) const
{
    Matrix result;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result.m[j * 4 + i] =
                this->m[0 * 4 + i] * rhs.m[j * 4 + 0] +
                this->m[1 * 4 + i] * rhs.m[j * 4 + 1] +
                this->m[2 * 4 + i] * rhs.m[j * 4 + 2] +
                this->m[3 * 4 + i] * rhs.m[j * 4 + 3];
        }
    }
    return result;
}

Matrix& Matrix::operator*=(const Matrix& rhs)
{
    *this = *this * rhs;
    return *this;
}

Matrix Matrix::CreateTranslation(const Vec2& offset)
{
    Matrix mat;
    mat.m[12] = offset.x;
    mat.m[13] = offset.y;
    return mat;
}

Matrix Matrix::CreateScale(const Vec2& scale)
{
    Matrix mat;
    mat.m[0] = scale.x;
    mat.m[5] = scale.y;
    return mat;
}

Matrix Matrix::CreateOrtho(float left, float right, float bottom, float top, float near, float far)
{
    Matrix mat;
    mat.m[0] = 2.0f / (right - left);
    mat.m[5] = 2.0f / (top - bottom);
    mat.m[10] = -2.0f / (far - near);
    mat.m[12] = -(right + left) / (right - left);
    mat.m[13] = -(top + bottom) / (top - bottom);
    mat.m[14] = -(far + near) / (far - near);
    return mat;
}

const float* Matrix::GetAsFloatPtr() const
{
    return m;
}
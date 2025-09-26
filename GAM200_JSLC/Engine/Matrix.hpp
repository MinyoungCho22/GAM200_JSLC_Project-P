#pragma once
#include "Vec2.hpp"

namespace Math
{
    // 4x4 �� �켱(Column-Major) ��� Ŭ����
    class Matrix
    {
    public:
        Matrix operator*(const Matrix& other) const;

        static Matrix CreateOrtho(float left, float right, float bottom, float top, float near, float far);
        static Matrix CreateTranslation(const Vec2& translate);
        static Matrix CreateScale(const Vec2& scale);
        static Matrix CreateIdentity(); // <-- �� ���� �߰��ϼ���.

        const float* const Ptr() const { return &m[0][0]; }

    private:
        // m[column][row] ������ ����
        float m[4][4];
    };
}
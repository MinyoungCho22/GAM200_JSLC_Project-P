#include "Matrix.hpp"
#include <cstring>

namespace Math
{
    Matrix Matrix::operator*(const Matrix& other) const
    {
        Matrix result{};
        for (int i = 0; i < 4; ++i) { // ��
            for (int j = 0; j < 4; ++j) { // ��
                for (int k = 0; k < 4; ++k) {
                    // �� �켱 ���� ����: result.m[j][i] = A[k][i] * B[j][k]
                    result.m[j][i] += m[k][i] * other.m[j][k];
                }
            }
        }
        return result;
    }

    Matrix Matrix::CreateOrtho(float left, float right, float bottom, float top, float near, float far)
    {
        Matrix mat{};
        mat.m[0][0] = 2.0f / (right - left);
        mat.m[1][1] = 2.0f / (top - bottom);
        mat.m[2][2] = -2.0f / (far - near);
        // [����] �� �켱 ��Ŀ� �´� �ùٸ� �ε����� ����
        mat.m[3][0] = -(right + left) / (right - left);
        mat.m[3][1] = -(top + bottom) / (top - bottom);
        mat.m[3][2] = -(far + near) / (far - near);
        mat.m[3][3] = 1.0f;
        return mat;
    }

    Matrix Matrix::CreateTranslation(const Vec2& translate)
    {
        Matrix mat{};
        mat.m[0][0] = 1.0f;
        mat.m[1][1] = 1.0f;
        mat.m[2][2] = 1.0f;
        mat.m[3][3] = 1.0f;
        // [����] �� �켱 ��Ŀ� �´� �ùٸ� �ε����� ����
        mat.m[3][0] = translate.x;
        mat.m[3][1] = translate.y;
        return mat;
    }

    Matrix Matrix::CreateScale(const Vec2& scale)
    {
        Matrix mat{};
        mat.m[0][0] = scale.x;
        mat.m[1][1] = scale.y;
        mat.m[2][2] = 1.0f;
        mat.m[3][3] = 1.0f;
        return mat;
    }

    Matrix Matrix::CreateIdentity()
    {
        Matrix mat{}; // ��� ��Ҹ� 0���� �ʱ�ȭ
        mat.m[0][0] = 1.0f;
        mat.m[1][1] = 1.0f;
        mat.m[2][2] = 1.0f;
        mat.m[3][3] = 1.0f;
        return mat;
    }
}
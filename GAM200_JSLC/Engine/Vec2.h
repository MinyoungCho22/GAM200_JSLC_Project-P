#pragma once

struct Vec2
{
    float x, y;

    // ������
    Vec2();
    Vec2(float x, float y);

    // ������ ���̸� ����ϴ� �Լ�
    float Length() const;
    float LengthSq() const; // ���� ���̴� ��Ʈ ����� ���� �� �����ϴ�.

    // ���͸� ����ȭ�ϴ� �Լ� (���̸� 1�� ����)
    Vec2& Normalize();
    Vec2 GetNormalized() const;

    // �⺻ ������ �����ε�
    Vec2& operator+=(const Vec2& rhs);
    Vec2& operator-=(const Vec2& rhs);
    Vec2& operator*=(float scalar);
    Vec2& operator/=(float scalar);
};

// ��� �Լ��� �ƴ� �Ϲ� �Լ��� ������ �����ε� (Vec2 + Vec2 ����)
Vec2 operator+(const Vec2& lhs, const Vec2& rhs);
Vec2 operator-(const Vec2& lhs, const Vec2& rhs);
Vec2 operator*(const Vec2& lhs, float scalar);
Vec2 operator*(float scalar, const Vec2& rhs);
Vec2 operator/(const Vec2& lhs, float scalar);
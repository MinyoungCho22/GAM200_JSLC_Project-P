#pragma once
#include "../Engine/Vec2.h" // ���� ���� Vec2 ����� �����մϴ�.

// ���� ����
struct GLFWwindow;

class Player
{
public:
    // Init �Լ��� �Ű����� Ÿ���� Vec2�� ����
    void Init(Vec2 startPos);
    void Update(double dt, GLFWwindow* p_window);
    void Draw() const;

private:
    // ��� �������� Ÿ���� ��� Vec2�� ����
    Vec2 position;
    Vec2 velocity;
    Vec2 size;

    bool is_on_ground = false;
};
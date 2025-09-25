#pragma once
#include "../Engine/Vec2.h"

class Shader;
struct GLFWwindow;

class Player
{
public:
    void Init(Vec2 startPos, const char* texturePath);
    void Update(double dt, GLFWwindow* p_window);
    void Draw(const Shader& shader) const;
    void Shutdown();

private:
    // �⺻ ����
    Vec2 position;
    Vec2 velocity;
    Vec2 size;
    bool is_on_ground = false;
    int last_move_direction = 1; // 1: ������, -1: ����

    // ��ũ���� ����
    bool is_crouching = false;
    Vec2 original_size; // ���� ũ�⸦ �����ϱ� ���� ����

    // ��� ����
    bool is_dashing = false;
    float dash_timer = 0.0f;

    // �÷��̾� �ɷ�ġ
    float move_speed = 300.0f;
    float jump_velocity = 500.0f;
    float dash_speed = 900.0f;
    float dash_duration = 0.15f;

    // ������ ����
    unsigned int VAO = 0;
    unsigned int VBO = 0;
    unsigned int textureID = 0;
};
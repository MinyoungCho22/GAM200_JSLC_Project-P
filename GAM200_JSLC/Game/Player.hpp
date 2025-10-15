#pragma once
#include "../Engine/Vec2.hpp"
#include "../Game/PurseCore.hpp"      // [�߰�] PulseCore ����� �����մϴ�.

class Shader;

class Player
{
public:
    void Init(Math::Vec2 startPos, const char* texturePath);
    void Update(double dt);
    void Draw(const Shader& shader) const;
    void Shutdown();

    // �ൿ ��� �Լ���
    void MoveLeft();
    void MoveRight();
    void Jump();
    void Crouch();
    void StopCrouch();
    void Dash();

    // [�߰�] Collision Check�� ���� Get �Լ���
    Math::Vec2 GetPosition() const { return position; }
    Math::Vec2 GetSize() const { return size; }

    // [�߰�] Player�� PulseCore�� ������ �� �ִ� �Լ�
    PulseCore& GetPulseCore() { return m_pulseCore; }

private:
    // �⺻ ����
    Math::Vec2 position;
    Math::Vec2 velocity;
    Math::Vec2 size;
    bool is_on_ground = false;
    int last_move_direction = 1; // 1: ������, -1: ����

    // [�߰�] PulseCore ��� ����
    PulseCore m_pulseCore;

    // ��ũ���� ����
    bool is_crouching = false;
    Math::Vec2 original_size;

    // ��� ����
    bool is_dashing = false;
    float dash_timer = 0.0f;

    // �÷��̾� �ɷ�ġ
    float move_speed = 300.0f;
    float jump_velocity = 600.0f;
    float dash_speed = 900.0f;
    float dash_duration = 0.15f;

    // ������ ����
    unsigned int VAO = 0;
    unsigned int VBO = 0;
    unsigned int textureID = 0;
};
#pragma once
#include "../Engine/Vec2.hpp"
#include "../Game/PulseCore.hpp"

class Shader;

class Player
{
public:
    void Init(Math::Vec2 startPos, const char* texturePath);
    void Update(double dt);
    void Draw(const Shader& shader) const;
    void Shutdown();

    void MoveLeft();
    void MoveRight();
    void Jump();
    void Crouch();
    void StopCrouch();
    void Dash();

    // [추가] 피해를 입는 함수
    void TakeDamage(float amount);

    Math::Vec2 GetPosition() const { return position; }
    Math::Vec2 GetSize() const { return size; }
    PulseCore& GetPulseCore() { return m_pulseCore; }
    bool IsDashing() const { return is_dashing; }

private:
    Math::Vec2 position;
    Math::Vec2 velocity;
    Math::Vec2 size;
    bool is_on_ground = false;
    int last_move_direction = 1;

    PulseCore m_pulseCore{ 100.f, 20.f };

    bool is_crouching = false;
    Math::Vec2 original_size;

    bool is_dashing = false;
    float dash_timer = 0.0f;

    // [추가] 무적 상태 관리 변수
    bool  m_isInvincible = false;
    float m_invincibilityTimer = 0.0f;
    const float m_invincibilityDuration = 3.0f; // 3초 무적

    float move_speed = 300.0f;
    float jump_velocity = 600.0f;
    float dash_speed = 900.0f;
    float dash_duration = 0.15f;

    unsigned int VAO = 0;
    unsigned int VBO = 0;
    unsigned int textureID = 0;
};
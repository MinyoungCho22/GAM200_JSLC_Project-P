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

    void TakeDamage(float amount);
    void SetPosition(Math::Vec2 new_pos);
    Math::Vec2 GetPosition() const { return position; }
    Math::Vec2 GetSize() const { return size; }
    PulseCore& GetPulseCore() { return m_pulseCore; }
    bool IsDashing() const { return is_dashing; }

private:
    Math::Vec2 position{};
    Math::Vec2 velocity{};
    Math::Vec2 size{};
    bool is_on_ground = false;
    int last_move_direction = 1;

    PulseCore m_pulseCore{ 100.f, 20.f };

    bool is_crouching = false;
    Math::Vec2 original_size{};

    bool is_dashing = false;
    float dash_timer = 0.0f;

    bool  m_isInvincible = false;
    float m_invincibilityTimer = 0.0f;
    const float m_invincibilityDuration = 3.0f;

    float move_speed = 300.0f;
    float jump_velocity = 600.0f;
    float dash_speed = 900.0f;
    float dash_duration = 0.15f;

    unsigned int VAO = 0;
    unsigned int VBO = 0;
    unsigned int textureID = 0;

    
    int m_tex_width = 0;         // 전체 텍스처 시트의 너비
    int m_tex_height = 0;        // 전체 텍스처 시트의 높이
    int m_frame_width = 0;       // 프레임 하나의 너비

    bool m_is_flipped = false;   // 좌우 반전 여부

    int m_anim_total_frames = 7; // 애니메이션 총 프레임 수
    int m_anim_current_frame = 0; // 현재 프레임 인덱스
    float m_anim_timer = 0.0f;
    float m_anim_frame_duration = 0.1f; // 1프레임당 0.1초 (10 FPS)
};
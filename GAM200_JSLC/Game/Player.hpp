// Player.hpp

#pragma once
#include "../Engine/Vec2.hpp"
#include "../Game/PulseCore.hpp"
#include <string>

class Shader;


enum class AnimationState
{
    Idle,
    Walking,
    Jumping,
    Crouching,
    Dashing
};


struct AnimationData
{
    unsigned int textureID = 0;
    int texWidth = 0;
    int texHeight = 0;
    int frameWidth = 0;
    int totalFrames = 0;
    int currentFrame = 0;
    float timer = 0.0f;
    float frameDuration = 0.1f;

    void Update(float dt);
    void Reset();
};

class Player
{
public:
    void Init(Math::Vec2 startPos);
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

    bool LoadAnimation(AnimationState state, const char* texturePath, int totalFrames, float frameDuration);


    AnimationState DetermineAnimationState() const;


    AnimationData m_animations[5]; // Idle, Walking, Jumping, Crouching, Dashing
    AnimationState m_currentAnimState = AnimationState::Idle;

    // 기존 멤버 변수들
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
    bool m_isInvincible = false;
    float m_invincibilityTimer = 0.0f;
    const float m_invincibilityDuration = 2.0f;

    float move_speed = 300.0f;
    float jump_velocity = 600.0f;
    float dash_speed = 900.0f;
    float dash_duration = 0.15f;

    unsigned int VAO = 0;
    unsigned int VBO = 0;

    bool m_is_flipped = false;
};
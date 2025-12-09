//Player.hpp

#pragma once
#include "../Engine/Vec2.hpp"
#include "../Game/PulseCore.hpp"
#include "../Engine/Input.hpp" 
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
    void Update(double dt, Input::Input& input);
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
    void SetCurrentGroundLevel(float newGroundLevel);
    void ResetVelocity();
    void SetOnGround(bool onGround);
    Math::Vec2 GetPosition() const { return position; }
    Math::Vec2 GetSize() const { return size; }
    Math::Vec2 GetHitboxSize() const;
    Math::Vec2 GetHitboxCenter() const;
    PulseCore& GetPulseCore() { return m_pulseCore; }
    bool IsDashing() const { return is_dashing; }
    bool IsFacingRight() const;
    bool IsCrouching() const { return is_crouching; }
    bool IsDead() const;

private:
    bool LoadAnimation(AnimationState state, const char* texturePath, int totalFrames, float frameDuration);
    AnimationState DetermineAnimationState() const;
    AnimationData m_animations[5];
    AnimationState m_currentAnimState = AnimationState::Idle;
    Math::Vec2 position{};
    Math::Vec2 velocity{};
    Math::Vec2 size{};
    bool is_on_ground = false;
    int last_move_direction = 1;
    PulseCore m_pulseCore{ 100.f, 20.f };
    bool is_crouching = false;
    bool m_crouchAnimationFinished = false;
    Math::Vec2 original_size{};
    bool is_dashing = false;
    float dash_timer = 0.0f;
    bool m_isInvincible = false;
    float m_invincibilityTimer = 0.0f;
    const float m_invincibilityDuration = 2.0f;
    float m_currentGroundLevel = 180.0f;
    float move_speed = 300.0f;
    float jump_velocity = 900.0f;
    float dash_speed = 900.0f;
    float dash_duration = 0.15f;
    unsigned int VAO = 0;
    unsigned int VBO = 0;
    bool m_is_flipped = false;
};
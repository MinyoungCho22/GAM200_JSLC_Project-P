//Robot.hpp

#pragma once
#include "../Engine/Vec2.hpp"
#include "../Engine/Sound.hpp"
#include <vector>

class Shader;
class DebugRenderer;
class Player;

struct ObstacleInfo {
    Math::Vec2 pos;
    Math::Vec2 size;
};

enum class RobotState {
    Patrol,
    Chase,
    Retreat,
    Windup,
    Attack,
    Recover,
    Stagger,
    Dead
};

enum class AttackType {
    None,
    LowSweep,
    HighSweep
};

class Robot
{
public:
    void Init(Math::Vec2 startPos);
    void Reset();
    /// Scales HP, patrol/chase speed, and shortens pre-attack windup (Underground map only).
    void ApplyUndergroundDifficultyBoost();
    void Update(double dt, Player& player, const std::vector<ObstacleInfo>& obstacles, float mapMinX, float mapMaxX);
    void Draw(const Shader& shader) const;
    void DrawOutline(const Shader& outlineShader) const;
    void DrawGauge(Shader& colorShader, DebugRenderer& debugRenderer) const;
    void DrawAlert(Shader& colorShader, DebugRenderer& debugRenderer) const;
    void Shutdown();

    Math::Vec2 GetPosition() const { return m_position; }
    Math::Vec2 GetSize() const { return m_size; }
    bool IsDead() const { return m_state == RobotState::Dead; }
    void TakeDamage(float amount);
    
    // Debug setters for ImGui
    void SetPosition(const Math::Vec2& pos) { m_position = pos; }
    void SetSize(const Math::Vec2& size) { m_size = size; }
    void SetHP(float hp) { m_hp = hp; }
    void SetMaxHP(float maxHp) { m_maxHp = maxHp; }
    void SetDirectionX(float dir) { m_directionX = dir; }
    void SetState(RobotState state) { m_state = state; }
    
    // Debug getters for ImGui
    float GetHP() const { return m_hp; }
    float GetMaxHP() const { return m_maxHp; }
    float GetDirectionX() const { return m_directionX; }
    RobotState GetState() const { return m_state; }
    Math::Vec2 GetVelocity() const { return m_velocity; }
    float GetPatrolSpeed() const { return m_patrolSpeed; }
    float GetChaseSpeed() const { return m_chaseSpeed; }
    float GetDetectionRange() const { return DETECTION_RANGE; }
    float GetAttackRange() const { return ATTACK_RANGE; }

private:
    void DecideAttackPattern();

    unsigned int LoadTexture(const char* path);

    unsigned int m_textureID = 0;

    unsigned int m_textureHighID = 0;
    unsigned int m_textureLowID = 0;

    unsigned int m_VAO = 0;
    unsigned int m_VBO = 0;

    Math::Vec2 m_position;
    Math::Vec2 m_size;
    Math::Vec2 m_velocity;

    float m_hp = 100.0f;
    float m_maxHp = 100.0f;
    bool m_isOnGround = false;

    RobotState m_state = RobotState::Patrol;
    float m_stateTimer = 0.0f;
    float m_directionX = -1.0f;

    AttackType m_currentAttack = AttackType::None;
    AttackType m_lastAttack = AttackType::None;
    int m_consecutiveAttackCount = 0;
    float m_attackCooldownTimer = 0.0f;
    float m_staggerCooldown = 0.0f;
    bool m_hasDealtDamage = false;

    float m_spawnX = 0.0f;
    Math::Vec2 m_spawnPos{};
    const float PATROL_RANGE = 1000.0f;

    const float GRAVITY = -2200.0f;
    float m_patrolSpeed = 140.0f;
    float m_chaseSpeed = 350.0f;
    float m_windupTime = 0.8f;
    const float DETECTION_RANGE = 500.0f;
    const float ATTACK_RANGE = 350.0f;
    const float STAGGER_COOLDOWN_DURATION = 3.0f;
    const float ATTACK_DURATION = 0.2f;
    const float RECOVER_TIME = 1.5f;

    Sound m_soundHigh;
    Sound m_soundLow;
    bool m_hasPlayedAttackSound = false;
};
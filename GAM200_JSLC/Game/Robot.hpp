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
    void Update(double dt, Player& player, const std::vector<ObstacleInfo>& obstacles, float mapMinX, float mapMaxX);
    void Draw(const Shader& shader) const;
    void DrawGauge(Shader& colorShader, DebugRenderer& debugRenderer) const;
    void DrawAlert(Shader& colorShader, DebugRenderer& debugRenderer) const;
    void Shutdown();

    Math::Vec2 GetPosition() const { return m_position; }
    Math::Vec2 GetSize() const { return m_size; }
    bool IsDead() const { return m_state == RobotState::Dead; }
    void TakeDamage(float amount);

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

    const float GRAVITY = -1500.0f;
    const float PATROL_SPEED = 100.0f;
    const float CHASE_SPEED = 250.0f;
    const float DETECTION_RANGE = 800.0f;
    const float ATTACK_RANGE = 350.0f;
    const float STAGGER_COOLDOWN_DURATION = 3.0f;
    const float WINDUP_TIME = 0.8f;
    const float ATTACK_DURATION = 0.2f;
    const float RECOVER_TIME = 1.5f;

    Sound m_soundHigh;
    Sound m_soundLow;
    bool m_hasPlayedAttackSound = false;
};
//Drone.hpp

#pragma once
#include "../Engine/Vec2.hpp"
#include "../Engine/Sound.hpp"

class Shader;
class Player;
class DebugRenderer;

class Drone
{
public:
    void Init(Math::Vec2 startPos, const char* texturePath, bool isTracer = false);
    void Update(double dt, const Player& player, Math::Vec2 playerHitboxSize, bool isPlayerHiding);
    void Draw(const Shader& shader) const;
    void DrawRadar(const Shader& colorShader, DebugRenderer& debugRenderer) const;
    void DrawGauge(Shader& colorShader, DebugRenderer& debugRenderer) const;
    void Shutdown();

    bool ApplyDamage(float dt);
    void ResetDamageTimer();
    void SetBaseSpeed(float speed);

    Math::Vec2 GetPosition() const { return m_position; }
    Math::Vec2 GetSize() const { return m_size; }
    bool IsAttacking() const { return m_isAttacking; }
    bool IsDead() const { return m_isDead; }
    bool ShouldDealDamage() const { return m_shouldDealDamage; }
    void ResetDamageFlag() { m_shouldDealDamage = false; }
    bool IsHit() const { return m_isHit; }

    static constexpr float DETECTION_RANGE = 150.0f;
    static constexpr float DETECTION_RANGE_SQ = DETECTION_RANGE * DETECTION_RANGE;
    static constexpr float TIME_TO_DESTROY = 1.0f;

    // Debug access methods
    Math::Vec2 GetVelocity() const { return m_velocity; }
    void SetVelocity(const Math::Vec2& velocity) { m_velocity = velocity; }
    void SetPosition(const Math::Vec2& position) { m_position = position; }
    void SetSize(const Math::Vec2& size) { m_size = size; }
    float GetBaseSpeed() const { return m_baseSpeed; }
    void SetBaseSpeedDebug(float speed) { m_baseSpeed = speed; }
    void SetDead(bool dead) { m_isDead = dead; }
    void SetHit(bool hit) { m_isHit = hit; if (hit) m_hitTimer = 0.0f; }
    void SetAttacking(bool attacking) { m_isAttacking = attacking; }
    float GetDamageTimer() const { return m_damageTimer; }
    void SetDamageTimer(float timer) { m_damageTimer = timer; }
    float GetAttackRadius() const { return m_attackRadius; }
    void SetAttackRadius(float radius) { const_cast<float&>(m_attackRadius) = radius; }
    float GetAttackAngle() const { return m_attackAngle; }
    void SetAttackAngle(float angle) { m_attackAngle = angle; }
    int GetAttackDirection() const { return m_attackDirection; }
    void SetAttackDirection(int direction) { m_attackDirection = direction; }
    void ResetHitTimer() { m_hitTimer = 0.0f; }
    void ResetDamageTimerDebug() { m_damageTimer = 0.0f; }

    // Debug mode control
    void SetDebugMode(bool debug);
    bool IsDebugMode() const { return m_debugMode; }

private:
    void StartDeathSequence();

    Math::Vec2 m_position;
    Math::Vec2 m_velocity;
    Math::Vec2 m_direction;
    Math::Vec2 m_size;

    bool m_isHit = false;
    float m_hitTimer = 0.0f;
    const float m_hitDuration = 2.0f;
    float m_hitRotation = 0.0f;
    float m_fallSpeed = 0.0f;
    bool m_isDead = false;

    float m_damageTimer = 0.0f;
    bool m_isTracer = false;
    bool m_isChasing = false;
    float m_lostTimer = 0.0f;

    float m_searchRotation = 0.0f;
    float m_searchMaxAngle = 30.0f;
    int m_searchDir = 1;

    bool m_isAttacking = false;
    bool m_shouldDealDamage = false;
    float m_attackCooldown = 0.0f;
    const float m_attackCooldownDuration = 2.0f;
    float m_attackTimer = 0.0f;
    const float m_attackDuration = 1.0f;
    Math::Vec2 m_attackStartPos;
    Math::Vec2 m_attackCenter;
    float m_attackAngle = 0.0f;
    const float m_attackRadius = 100.0f;
    int m_attackDirection = 1;

    float m_radarAngle = 0.0f;
    const float m_radarRotationSpeed = 200.0f;
    const float m_radarLength = 150.0f;

    float m_baseSpeed = 200.0f;
    float m_currentSpeed = 200.0f;
    float m_moveTimer = 0.0f;

    float m_acceleration = 800.0f;

    float m_baseY = 0.0f;
    float m_bobTimer = 0.0f;
    float m_groundLevel = 180.0f;
    unsigned int VAO = 0, VBO = 0, textureID = 0;

    bool m_debugMode = false; // When true, AI is disabled for manual positioning
    float m_debugExitTimer = 0.0f; // Timer to delay AI restart after exiting debug mode

    Sound m_moveSound;
};
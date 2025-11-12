//Drone.hpp

#pragma once
#include "../Engine/Vec2.hpp"

class Shader;
class Player;
class DebugRenderer;

class Drone
{
public:
    void Init(Math::Vec2 startPos, const char* texturePath);
    void Update(double dt, const Player& player, Math::Vec2 playerHitboxSize);
    void Draw(const Shader& shader) const;
    void DrawRadar(const Shader& colorShader, DebugRenderer& debugRenderer) const;
    void Shutdown();
    void TakeHit();

    Math::Vec2 GetPosition() const { return m_position; }
    Math::Vec2 GetSize() const { return m_size; }
    bool IsAttacking() const { return m_isAttacking; }
    bool IsDead() const { return m_isDead; }
    bool ShouldDealDamage() const { return m_shouldDealDamage; }
    void ResetDamageFlag() { m_shouldDealDamage = false; }

    static constexpr float DETECTION_RANGE = 100.0f;
    static constexpr float DETECTION_RANGE_SQ = DETECTION_RANGE * DETECTION_RANGE;

private:
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

    bool m_isAttacking = false;
    bool m_shouldDealDamage = false;
    float m_attackCooldown = 0.0f;
    const float m_attackCooldownDuration = 1.0f;
    float m_attackTimer = 0.0f;
    const float m_attackDuration = 1.0f;
    Math::Vec2 m_attackStartPos;
    Math::Vec2 m_attackCenter;
    float m_attackAngle = 0.0f;
    const float m_attackRadius = 100.0f;
    int m_attackDirection = 1;

    float m_radarAngle = 0.0f;
    const float m_radarRotationSpeed = 200.0f;
    const float m_radarLength = 100.0f;

    float m_speed = 200.0f;
    float m_moveTimer = 0.0f;

    float m_baseY = 0.0f;
    float m_bobTimer = 0.0f;

    unsigned int VAO = 0, VBO = 0, textureID = 0;
};
//Drone.hpp

#pragma once
#include "../Engine/Vec2.hpp"
#include "../Engine/Sound.hpp"
#include <string>

class Shader;
class Player;
class DebugRenderer;

class Drone
{
public:
    void Init(Math::Vec2 startPos, const char* texturePath, bool isTracer = false);
    void Reset();
    void SetSirenMapDrone(bool v) { m_sirenMapDrone = v; }
    /// isPlayerUndetectable: 히딩/펄스박스 등으로 레이더 추적 불가
    /// sirenTracerJamEvade: 사이렌 추적 드론만 — 미포착 시 좌우 흔들림 후 랜덤 방향 이탈
    /// sirenTracerSpeedMul: 사이렌 가동 중 1, 파괴 후 추적 감속 시 1 미만
    void Update(double dt, const Player& player, Math::Vec2 playerHitboxSize, bool isPlayerUndetectable,
                bool sirenTracerJamEvade = false, float sirenTracerSpeedMul = 1.f,
                float sirenTracerTrainAssistMul = 1.f);
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
    bool IsTracer() const { return m_isTracer; }
    bool IsTraceReinforcement() const { return m_isTracer && m_texturePath.find("RedDrone.png") != std::string::npos; }
    bool ShouldDealDamage() const { return m_shouldDealDamage; }
    void ResetDamageFlag() { m_shouldDealDamage = false; }
    bool IsHit() const { return m_isHit; }
    void DisableForCheckpoint()
    {
        m_isDead = true;
        m_isHit = false;
        m_isChasing = false;
        m_isAttacking = false;
        m_shouldDealDamage = false;
        m_corpseRestTimer = 0.f;
        m_corpseFadeAlpha = 0.f;
        m_moveSound.Stop();
    }

    bool IsStunned() const { return m_stunTimer > 0.f; }
    void ApplyStun(float duration);
    // velocity: initial impulse direction×speed; delay: seconds before slide starts (domino wave)
    void ApplyKnockback(Math::Vec2 velocity, float delay)
    {
        if (m_isDead) return;
        m_knockbackVelocity = velocity;
        m_knockbackDelay    = delay;
        m_knockbackTimer    = 0.f;
    }
    // Queue damage to be applied after the slide animation (delay = knockback delay + slide time)
    void QueueDamage(float damage, float delay)
    {
        if (m_isDead) return;
        m_pendingDamage = damage;
        m_damageDelay   = delay;
    }

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
    /// Third_Third 자동차 칸: 파이프 근처 고정 호버 (Train이 매 프레임 앵커 갱신)
    void SetCarTransportHover(bool v) { m_carTransportHover = v; }
    bool IsCarTransportHover() const { return m_carTransportHover; }
    /// 체크포인트 리스폰 시 Reset() 후에도 호버 모드 유지
    void SetCarTransportPersistHover(bool v) { m_carTransportPersistHover = v; m_carTransportHover = v; }
    bool IsCarTransportPersistHover() const { return m_carTransportPersistHover; }
    /// 펄스/넉백 후: 고정 호버 대신 플레이어를 느리게 추적
    bool IsCarTransportAggroChase() const { return m_carTransportAggroChase; }
    void SetCarTransportAnchorWorld(const Math::Vec2& w) { m_carTransportAnchorWorld = w; }
    void SetCarTransportJamConfused(bool v) { m_carTransportJamConfused = v; }
    float GetCarTransportBobPhase() const { return m_carTransportBobPhase; }
    void  SetCarTransportBobPhase(float p) { m_carTransportBobPhase = p; }
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
    
    // HP system
    float GetHP() const { return m_hp; }
    void SetHP(float hp) { 
        m_hp = hp; 
        if (m_hp <= 0.0f) { 
            m_hp = 0.0f; 
            StartDeathSequence();
        } else if (m_isDead && m_hp > 0.0f) {
            // Revive if HP is set above 0
            m_isDead = false;
            m_isHit = false;
            m_corpseRestTimer = 0.f;
            m_corpseFadeAlpha = 0.f;
        }
    }
    float GetMaxHP() const { return m_maxHP; }
    void SetMaxHP(float maxHP) { m_maxHP = maxHP; }
    void TakeDamage(float damage) { SetHP(m_hp - damage); }

    // Debug mode control
    void SetDebugMode(bool debug);
    bool IsDebugMode() const { return m_debugMode; }

    /// Train 전용: 이 칸(1~5)에 묶인 전투 드론 — 다른 칸 플레이어에게 피해 안 줌
    void SetTrainCarSegment(int car1To5) { m_trainCarSegment = car1To5; }
    int GetTrainCarSegment() const { return m_trainCarSegment; }
    /// TraceSystem 경고 단계(0~2): 추적 속도·거리 보정에 사용
    void SetTracerHeatLevel(int level) { m_tracerHeatLevel = level < 0 ? 0 : level; }
    int GetTracerHeatLevel() const { return m_tracerHeatLevel; }

private:
    void StartDeathSequence();

    Math::Vec2 m_spawnPos;
    Math::Vec2 m_position;
    Math::Vec2 m_velocity;
    Math::Vec2 m_direction;
    Math::Vec2 m_size;

    float      m_stunTimer        = 0.f;
    Math::Vec2 m_knockbackVelocity{ 0.f, 0.f };
    float      m_knockbackDelay   = 0.f;   // countdown before slide kicks in
    float      m_knockbackTimer   = 0.f;   // remaining slide duration
    float      m_pendingDamage    = 0.f;   // damage queued to apply after knockback
    float      m_damageDelay      = 0.f;   // countdown before pending damage applies

    bool m_isHit = false;
    /// 시체: 착지 후 대기(초) → 알파 페이드. 즉사 시 둘 다 0이면 드로우 생략.
    float m_corpseRestTimer = 0.f;
    float m_corpseFadeAlpha = 0.f;
    float m_hitTimer = 0.0f;
    const float m_hitDuration = 2.0f;
    float m_hitRotation = 0.0f;
    float m_fallSpeed = 0.0f;
    bool m_isDead = false;

    float m_damageTimer = 0.0f;
    float m_hp = 100.0f;
    float m_maxHP = 100.0f;
    bool m_isTracer = false;
    std::string m_texturePath;
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

    // Train 사이렌 맵 전용: 히딩/펄스박스 미포착 시 흔들림 → 랜덤 방향 이탈
    float      m_jamFleeTimer              = 0.f;
    bool       m_jamScheduleFleeAfterWobble = false;
    Math::Vec2 m_jamFleeDir{ 1.f, 0.f };

    bool  m_sirenMapDrone = false;
    /// Q/펄스 피격 직후: 히딩/펄스박스여도 잠시 플레이어를 추적 (잘못된 방향 이탈 방지)
    float m_sirenPulseRevealTimer = 0.f;
    /// Q 펄스 넉백 직후 추적 가속 (초)
    float m_sirenPulseAggroTimer  = 0.f;
    float m_hitHorzVel    = 0.f;
    int   m_hitWindSign   = 1;

    Sound m_moveSound;

    bool       m_carTransportHover          = false;
    bool       m_carTransportPersistHover   = false;
    bool       m_carTransportAggroChase    = false;
    bool       m_carTransportJamConfused     = false;
    Math::Vec2 m_carTransportAnchorWorld{};
    float      m_carTransportBobPhase       = 0.f;

    int m_trainCarSegment = 0;
    /// 0 = 일반/입장 트레이서, 1~2 = TraceSystem 경고 단계
    int m_tracerHeatLevel = 0;
};
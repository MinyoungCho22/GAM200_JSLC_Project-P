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
    /// Train FourthCar 구간: 체만 지하 배율 수준으로, 추적 속도는 낮춤
    void ApplyTrainEncounterDifficulty();
    /// Q 펄스: 넉백 + HP (드론 주입과 비슷한 느낌)
    void ApplyPulseImpact(Math::Vec2 impulse, float damage);
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
    /// 체크포인트/리셋 시 복귀할 월드 위치(Init 직후 SetPosition으로 보정한 경우 갱신 필요)
    void SetSpawnPosition(Math::Vec2 worldPos) { m_spawnPos = worldPos; m_spawnX = worldPos.x; }
    void SetSize(const Math::Vec2& size) { m_size = size; }
    void SetHP(float hp) { m_hp = hp; }
    void SetMaxHP(float maxHp) { m_maxHp = maxHp; }
    void SetDirectionX(float dir) { m_directionX = dir; }
    void SetState(RobotState state) { m_state = state; }
    /// Train 등 맵별 바닥 Y (발 한계). 기본값은 Underground 근처 고정 바닥.
    void SetGroundLimitY(float y) { m_groundLimitY = y; }
    
    // Debug getters for ImGui
    float GetHP() const { return m_hp; }
    float GetMaxHP() const { return m_maxHp; }
    float GetDirectionX() const { return m_directionX; }
    RobotState GetState() const { return m_state; }
    Math::Vec2 GetVelocity() const { return m_velocity; }
    void SetVelocity(const Math::Vec2& v) { m_velocity = v; }
    float GetPatrolSpeed() const { return m_patrolSpeed; }
    float GetChaseSpeed() const { return m_chaseSpeed; }
    float GetDetectionRange() const { return DETECTION_RANGE; }
    float GetAttackRange() const { return ATTACK_RANGE; }

    /// Train: 이 칸 전투만 플레이어에게 피해 (다른 칸 분리)
    void SetTrainCarSegment(int car1To5) { m_trainCarSegment = car1To5; }
    int GetTrainCarSegment() const { return m_trainCarSegment; }
    /// 열차 평판 위 패트롤 + 컨테이너 점프; `GetTrainCarSegment()`로 칸 구분
    void SetTrainDeckPatrol(bool v) { m_trainDeckPatrol = v; }
    bool IsTrainDeckPatrol() const { return m_trainDeckPatrol; }
    void SetUsePatrolWorldClamp(bool v) { m_usePatrolWorldClamp = v; }
    void SetPatrolWorldClamp(float minX, float maxX)
    {
        m_patrolWorldMinX = minX;
        m_patrolWorldMaxX = maxX;
    }
    void SetAllowTrainCombatVsPlayer(bool v) { m_allowTrainCombatVsPlayer = v; }
    bool IsOnGround() const { return m_isOnGround; }
    float GetTrainDetectAlertTimer() const { return m_trainDetectAlertTimer; }
    void DrawTrainDetectAlert(Shader& colorShader, DebugRenderer& debugRenderer) const;

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
    /// Stagger 중 펄스 넉백: 수평 미끄러짐 유지
    float m_horzExternalImpulseTimer = 0.0f;

    float m_spawnX = 0.0f;
    Math::Vec2 m_spawnPos{};
    const float PATROL_RANGE = 1000.0f;

    const float GRAVITY = -2200.0f;
    float m_patrolSpeed = 140.0f;
    float m_chaseSpeed = 350.0f;
    float m_groundLimitY = -1910.0f;
    float m_windupTime = 0.8f;
    const float DETECTION_RANGE = 500.0f;
    const float ATTACK_RANGE = 350.0f;
    const float STAGGER_COOLDOWN_DURATION = 3.0f;
    const float ATTACK_DURATION = 0.2f;
    const float RECOVER_TIME = 1.5f;

    Sound m_soundHigh;
    Sound m_soundLow;
    bool m_hasPlayedAttackSound = false;

    int  m_trainCarSegment = 0;
    bool m_trainDeckPatrol = false;
    bool m_usePatrolWorldClamp    = false;
    float m_patrolWorldMinX       = 0.f;
    float m_patrolWorldMaxX       = 0.f;
    bool m_allowTrainCombatVsPlayer = true;
    float m_trainDetectAlertTimer = 0.f;
};
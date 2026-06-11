#pragma once
#include "../Engine/Vec2.hpp"
#include "Background.hpp"
#include "PulseSource.hpp"
#include "Player.hpp"
#include <memory>
#include <vector>

class Shader;
class DebugRenderer;
class DroneManager;

enum class BossState
{
    Normal,
    Weakened,
    Defeated
};

struct OverloadDevice
{
    Math::Vec2 pos;
    Math::Vec2 size;
    float charge = 0.0f;
    bool isOverloaded = false;
    float pulseAttackTimer = 0.0f;
    bool pulseAttackActive = false;
};

struct BossProjectile
{
    Math::Vec2 pos;
    Math::Vec2 vel;
    bool active = false;
    float radius = 16.0f;
};

struct PulseLockAttack
{
    Math::Vec2 targetPos;
    float warningTimer = 0.0f;
    float maxWarningTime = 1.2f;
    bool active = false;
    float explosionRadius = 120.0f;
    bool triggeredExplosion = false;
    float explosionVisualTimer = 0.0f;
};

class Final
{
public:
    static constexpr float MIN_X  = 50000.0f;
    static constexpr float MIN_Y  = -2000.0f;
    static constexpr float HEIGHT = 1080.0f;

    struct Hitbox
    {
        Math::Vec2 pos;
        Math::Vec2 size;
        bool isYellow; // true = yellow/tall block, false = orange/low slab
    };

    struct FloorSweep
    {
        float x;
        float speed;
        float width;
        bool damagedPlayer;
    };

    void Initialize();
    void Reset();
    void Update(double dt, Player& player, Math::Vec2 playerHitboxSize, DroneManager& droneManager);
    void Draw(Shader& shader, Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW, const Math::Matrix& projection);
    void DrawDebug(Shader& colorShader, DebugRenderer& debugRenderer) const;
    void DrawPulseVents(Shader& shader, Shader& outlineShader, Math::Vec2 cameraPos, float viewHalfW);
    void DrawBossDefeatedEffect(Shader& outlineShader);
    // 과부하 장치 기둥(근접 시 보라 스캔라인) + 출구 게이트(보스 처치 시 보라 호흡 스캔라인) 효과
    void DrawFinalObjectEffects(Shader& outlineShader);
    void Shutdown();

    float GetMapWidth() const { return m_mapWidth; }
    float GetMapHeight() const { return HEIGHT; }

    const std::vector<Hitbox>& GetHitboxes() const { return m_hitboxes; }
    std::vector<PulseSource>& GetPulseSources() { return m_pulseSources; }
    const std::vector<PulseSource>& GetPulseSources() const { return m_pulseSources; }
    int GetActiveVentIndex() const { return m_activeVentIndex; }

    BossState GetBossState() const { return m_bossState; }
    void DamageBoss(float amount);
    float GetBossHealth() const { return m_bossHealth; }
    Math::Vec2 GetBossPosition() const { return m_boss.GetPosition(); }
    Math::Vec2 GetBossSize() const { return m_boss.GetHitboxSize(); }
    Player& GetBoss() { return m_boss; }
    const Player& GetBoss() const { return m_boss; }
    OverloadDevice* GetOverloadDevices() { return m_overloadDevices; }
    bool IsDeviceHovered(int idx, Math::Vec2 playerHbCenter, Math::Vec2 playerHbSize, Math::Vec2 mouseWorld) const;
    void UpdateDeviceInject(int idx, float dt, Player& player, bool godMode);
    bool IsBossHovered(Math::Vec2 mouseWorld) const;
    float ConsumeCameraShakeRequest();

private:
    void InitSkyVAO();
    void DrawFilledQuad(Shader& colorShader, Math::Vec2 center, Math::Vec2 size,
                        float r, float g, float b, float a = 1.0f) const;
    void DrawParallaxBackground(Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW) const;
    void DrawParallaxLayer(Shader& shader, Background& bg, Math::Vec2 cameraPos, float viewHalfW, float scrollSpeedFactor);

    std::unique_ptr<Background> m_final1;
    std::unique_ptr<Background> m_final2;
    std::unique_ptr<Background> m_cityLast;
    std::unique_ptr<Background> m_cityMiddle;
    std::unique_ptr<Background> m_cityFront;
    std::unique_ptr<Background> m_pulseLine;

    float m_mapWidth = 7920.0f;
    std::vector<Hitbox> m_hitboxes;
    std::vector<FloorSweep> m_sweeps;
    float m_sweepSpawnTimer = 0.0f;
    bool m_spawnDirectionAlternate = false;

    unsigned int m_skyVAO = 0;
    unsigned int m_skyVBO = 0;

    std::vector<PulseSource> m_pulseSources;
    std::unique_ptr<Background> m_pulseVentSprite;

    int m_activeVentIndex = -1;
    int m_nextVentIndex = -1;
    float m_ventTimer = 0.0f;
    static constexpr float VENT_ACTIVE_DURATION = 5.0f;
    static constexpr float VENT_CYCLE_DURATION = 8.0f;

    Player m_boss;
    BossState m_bossState = BossState::Normal;
    float m_bossHealth = 100.0f;
    float m_bossMaxHealth = 100.0f;
    OverloadDevice m_overloadDevices[2];
    std::vector<BossProjectile> m_bossProjectiles;
    float m_bossAttackTimer = 0.0f;
    float m_bossDroneSummonTimer = 0.0f;
    float m_bossSweepTimer = 0.0f;
    float m_slamGestureTimer = 0.0f;
    bool m_bossFlipped = false;

    std::unique_ptr<Background> m_overloadDeviceSprite;
    std::unique_ptr<Background> m_overlapSprite;       // 과부하 장치 기둥 오버레이 (Overlap.png)
    std::unique_ptr<Background> m_borderInsideSprite;  // 출구 게이트 프레임 (BorderInside.png)
    std::unique_ptr<Background> m_pulseMarkSprite;
    std::unique_ptr<Background> m_bossDroneProjectileSprite;
    std::unique_ptr<Background> m_realVentSprite;
    std::unique_ptr<Background> m_bossSummonCircleSprite;

    // 출구 게이트 시각 영역 (히트박스가 보스 처치 후 0이 되어도 유지)
    Math::Vec2 m_exitGatePos{};
    Math::Vec2 m_exitGateSize{};
    // 플레이어가 각 과부하 장치 기둥 근처인지 (보라 스캔라인 강조용)
    bool m_deviceProximity[2] = { false, false };
    bool m_ventProximity[3] = { false, false, false };

    std::unique_ptr<Background> m_pulseLineH;
    std::unique_ptr<Background> m_pulseLineV;
    std::unique_ptr<Background> m_pulseCornerNE;
    std::unique_ptr<Background> m_pulseCornerNW;
    std::unique_ptr<Background> m_pulseCornerSE;
    std::unique_ptr<Background> m_pulseCornerSW;

    PulseLockAttack m_pulseLock;
    float m_pulseLockCooldownTimer = 0.0f;
    float m_weakenedTimer = 0.0f;
    static constexpr float WEAKENED_DURATION = 8.0f;
    float m_cameraShakeRequest = 0.0f;
    bool m_firstVentActivated = false;
    bool m_bossEncountered = false;
    float m_droneDelayTimer = 0.0f;
    float m_bossDroneSummonEffectTimer = 0.0f;

    struct RecoilBomb
    {
        Math::Vec2 pos;
        Math::Vec2 vel;
        float timer = 1.5f;
        bool active = false;
    };

    struct BombExplosion
    {
        Math::Vec2 pos;
        float timer = 0.25f;
        bool active = false;
        bool damagedPlayer = false;
    };

    std::vector<RecoilBomb> m_recoilBombs;
    std::vector<BombExplosion> m_bombExplosions;
    float m_bossBattleTime = 0.0f;
    float m_bombThrowTimer = 0.0f;
};

//PulseManager.hpp

#pragma once
#include "../Engine/Vec2.hpp"
#include "Player.hpp" 
#include <vector>
#include <utility>
#include <memory>

class PulseSource;
class Shader;
class DebugRenderer;

class PulseManager
{
public:
    void Initialize();
    void Shutdown();

    void Update(Math::Vec2 playerHitboxCenter, Math::Vec2 playerHitboxSize,
        Player& player, std::vector<PulseSource>& roomSources,
        std::vector<PulseSource>& hallwaySources,
        std::vector<PulseSource>& rooftopSources,
        std::vector<PulseSource>& undergroundSources,
        std::vector<PulseSource>& trainSources,
        bool is_interact_key_pressed, double dt, Math::Vec2 mouseWorldPos);

    void UpdateAttackVFX(
        bool isAttacking,
        Math::Vec2 startPos,
        Math::Vec2 endPos,
        float attackSide = 1.0f
    );

    void DrawVFX(const Shader& shader) const;
    void SetVFXScale(float scale);

    void StartDetonationVFX(Math::Vec2 origin, float maxRadius,
                            const std::vector<std::pair<Math::Vec2, Math::Vec2>>& chainArcs = {});
    /// 매 프레임 플레이어 중심에 맞춤(기차 이동·점프 중에도 원이 플레이어에 붙음)
    void SyncDetonationOriginToPlayer(Math::Vec2 playerHitboxCenter);
    void DrawDetonationVFX(Shader& colorShader, DebugRenderer& debugRenderer) const;

private:
    AnimationData m_pulseAnim;
    unsigned int m_texLineH = 0;
    unsigned int m_texLineV = 0;
    unsigned int m_texCornerNE = 0;
    unsigned int m_texCornerNW = 0;
    unsigned int m_texCornerSE = 0;
    unsigned int m_texCornerSW = 0;

    unsigned int m_fluidTexID = 0;
    unsigned int m_pulseVAO = 0;
    unsigned int m_pulseVBO = 0;



    bool m_isAttacking = false;
    bool m_isCharging = false;
    float m_vfxTimer = 0.0f;
    double m_logTimer = 0.0;

    Math::Vec2 m_chargeStartPos;
    Math::Vec2 m_chargeEndPos;

    bool m_attackPathValid = false;
    float m_attackPathUpdateTimer = 0.0f;

    Math::Vec2 m_attackStartFrozen{};
    Math::Vec2 m_attackC1{};
    Math::Vec2 m_attackC2{};
    Math::Vec2 m_attackC3{};

    Math::Vec2 m_attackEndLive{};

    float m_lastDt = 0.016f;

    // Attack VFX timing (flow sync)
    float m_attackElapsed = 0.0f;
    float m_attackTravelTime = 0.12f;
    float m_attackTotalLen = 0.0f;
    Math::Vec2 m_attackPrevEnd = { 0.f, 0.f };
    bool  m_attackPacketActive = false;

    // Detonation VFX (Pulse Resonance Burst — expanding shockwave rings)
    bool       m_detonationActive = false;
    Math::Vec2 m_detonationOrigin{};
    float      m_detonationMaxRadius = 350.f;
    float      m_detonationTimer = 0.f;

    static constexpr float DETONATION_TOTAL_DURATION = 1.1f;
    static constexpr int   DETONATION_RING_COUNT     = 5;
    static constexpr float DETONATION_RING_STAGGER   = 0.07f;
    static constexpr float DETONATION_RING_DURATION  = 0.75f;

    // Chain arc VFX (Static-style chained pulse between drones)
    struct ChainArc
    {
        Math::Vec2 from{};
        Math::Vec2 to{};
        float timer = 0.f;
    };
    std::vector<ChainArc> m_chainArcs;
    static constexpr float CHAIN_ARC_DURATION = 0.40f;  // longer visibility for readability

    float m_vfxScale = 1.0f;
};
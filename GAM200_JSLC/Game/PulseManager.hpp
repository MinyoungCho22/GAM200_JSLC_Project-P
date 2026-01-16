//PulseManager.hpp

#pragma once
#include "../Engine/Vec2.hpp"
#include "Player.hpp" 
#include <vector>
#include <memory>

class PulseSource;
class Shader;

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
        bool is_interact_key_pressed, double dt);

    void UpdateAttackVFX(bool isAttacking, Math::Vec2 startPos, Math::Vec2 endPos);

    void DrawVFX(const Shader& shader) const;

private:
    AnimationData m_pulseAnim;
    unsigned int m_pulseVAO = 0;
    unsigned int m_pulseVBO = 0;

    bool m_isAttacking = false;
    bool m_isCharging = false;
    float m_vfxTimer = 0.0f;
    double m_logTimer = 0.0;
    Math::Vec2 m_attackStartPos;
    Math::Vec2 m_attackEndPos;

    Math::Vec2 m_chargeStartPos;
    Math::Vec2 m_chargeEndPos;
};
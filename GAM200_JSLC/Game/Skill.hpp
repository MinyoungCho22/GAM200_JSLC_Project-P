// Skill.hpp
// Pulse Resonance Burst — Q-skill unlocked when the Rooftop is first accessed.

#pragma once

#include "Font.hpp"            // CachedTextureInfo
#include "../Engine/Vec2.hpp"  // Math::Vec2
#include <utility>
#include <vector>

class Player;
class DroneManager;
class PulseManager;
class Train;
class Shader;
class ControlBindings;
namespace Input { class Input; }

class PulseDetonateSkill
{
public:
    void Initialize();

    // Call every frame inside GameplayState::Update().
    // rooftopDM  : drone manager owned by the Rooftop zone (may be nullptr)
    // tracerDM   : main drone manager (tracer drones)
    void Update(double dt,
                Player&            player,
                Math::Vec2         playerCenter,
                DroneManager*      rooftopDM,
                DroneManager*      tracerDM,
                PulseManager&      pulseManager,
                const ControlBindings& ctl,
                const Input::Input&    input,
                bool isGameOver,
                bool rooftopAccessed,
                bool isGodMode,
                DroneManager* trainDroneManager = nullptr,
                DroneManager* trainSirenDroneManager = nullptr,
                const Math::Vec2* trainDetonationOrigin = nullptr,
                const std::vector<std::pair<Math::Vec2, Math::Vec2>>* extraChainArcs = nullptr,
                DroneManager* trainCarTransportDroneManager = nullptr,
                Train* trainMapForBranchArcs = nullptr);

    // Bake the cooldown string into a GL texture — call during the text-update phase.
    void UpdateCooldownText(Font& font, Shader& fontShader);

    // Draw the cooldown label at (20, screenY) — call inside Draw() after font shader is bound.
    void DrawCooldownUI(Font& font, Shader& fontShader, float screenY) const;

    // Reset cooldown to 0 on player respawn (unlock state persists).
    void ResetCooldown();

    bool  IsUnlocked() const { return m_unlocked; }
    float GetCooldown() const { return m_cooldown; }
    const CachedTextureInfo& GetCooldownText() const { return m_cooldownText; }

private:
    bool  m_unlocked  = false;
    float m_cooldown  = 0.f;
    CachedTextureInfo m_cooldownText{};

    static constexpr float SKILL_COST     = 8.f;
    static constexpr float SKILL_RADIUS   = 535.f;
    static constexpr float SKILL_COOLDOWN = 3.f;
    static constexpr float STUN_DURATION  = 2.0f;
};

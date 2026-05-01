// Skill.cpp
// Pulse Resonance Burst — radial stun + chain lightning, unlocked on Rooftop.

#include "Skill.hpp"
#include "Player.hpp"
#include "DroneManager.hpp"
#include "PulseManager.hpp"
#include "PulseCore.hpp"
#include "../Engine/ControlBindings.hpp"
#include "../Engine/Logger.hpp"
#include <sstream>
#include <vector>
#include <utility>

// ── Lifecycle ────────────────────────────────────────────────────────────────

void PulseDetonateSkill::Initialize()
{
    m_unlocked     = false;
    m_cooldown     = 0.f;
    m_cooldownText = {};
}

// ── Per-frame update ─────────────────────────────────────────────────────────

void PulseDetonateSkill::Update(
    double dt,
    Player&                player,
    Math::Vec2             playerCenter,
    DroneManager*          rooftopDM,
    DroneManager*          tracerDM,
    PulseManager&          pulseManager,
    const ControlBindings& ctl,
    const Input::Input&    input,
    bool                   isGameOver,
    bool                   rooftopAccessed,
    bool                   isGodMode)
{
    // Unlock once the player enters the Rooftop for the first time
    if (rooftopAccessed && !m_unlocked)
        m_unlocked = true;

    if (m_cooldown > 0.f)
        m_cooldown -= static_cast<float>(dt);

    if (!m_unlocked || isGameOver)
        return;
    if (m_cooldown > 0.f)
        return;
    if (!ctl.IsActionTriggered(ControlAction::PulseDetonate, input))
        return;

    Pulse& pulse = player.GetPulseCore().getPulse();
    if (!isGodMode && pulse.Value() < SKILL_COST)
        return;

    // ── Activate ─────────────────────────────────────────────────────────────
    if (!isGodMode)
        pulse.spend(SKILL_COST);

    m_cooldown = SKILL_COOLDOWN;

    // Apply detonation to both drone groups and collect chain arcs for VFX
    std::vector<std::pair<Math::Vec2, Math::Vec2>> allArcs;

    if (rooftopDM)
    {
        auto arcs = rooftopDM->ApplyDetonation(playerCenter, SKILL_RADIUS, STUN_DURATION);
        allArcs.insert(allArcs.end(), arcs.begin(), arcs.end());
    }
    if (tracerDM)
    {
        auto arcs = tracerDM->ApplyDetonation(playerCenter, SKILL_RADIUS, STUN_DURATION);
        allArcs.insert(allArcs.end(), arcs.begin(), arcs.end());
    }

    pulseManager.StartDetonationVFX(playerCenter, SKILL_RADIUS, allArcs);

    Logger::Instance().Log(Logger::Severity::Info,
        "Pulse Resonance Burst activated at (%.1f, %.1f)",
        playerCenter.x, playerCenter.y);
}

// ── UI text ──────────────────────────────────────────────────────────────────

void PulseDetonateSkill::UpdateCooldownText(Font& font, Shader& fontShader)
{
    if (m_unlocked && m_cooldown > 0.f)
    {
        std::stringstream ss;
        ss.precision(1);
        ss << std::fixed << "[Q] " << m_cooldown << "s";
        m_cooldownText = font.PrintToTexture(fontShader, ss.str());
    }
    else
    {
        m_cooldownText = {};
    }
}

void PulseDetonateSkill::DrawCooldownUI(Font& font, Shader& fontShader, float screenY) const
{
    if (m_unlocked && m_cooldownText.textureID != 0)
        font.DrawBakedText(fontShader, m_cooldownText, { 20.f, screenY }, 32.0f);
}

// ── Reset ────────────────────────────────────────────────────────────────────

void PulseDetonateSkill::ResetCooldown()
{
    m_cooldown = 0.f;
    // m_unlocked intentionally NOT reset — persists across respawns
}

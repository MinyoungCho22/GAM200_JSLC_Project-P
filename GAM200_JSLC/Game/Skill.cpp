// Skill.cpp
// Pulse Resonance Burst — radial stun + chain lightning, unlocked on Rooftop.

#include "Skill.hpp"
#include "Player.hpp"
#include "DroneManager.hpp"
#include "PulseManager.hpp"
#include "PulseCore.hpp"
#include "Train.hpp"
#include "Underground.hpp"
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
    DroneManager*          undergroundDM,
    DroneManager*          tracerDM,
    PulseManager&          pulseManager,
    const ControlBindings& ctl,
    const Input::Input&    input,
    bool                   isGameOver,
    bool                   pulseBurstUnlockEligible,
    bool                   isGodMode,
    DroneManager*          trainDM,
    DroneManager*          trainSirenDM,
    const std::vector<std::pair<Math::Vec2, Math::Vec2>>* extraChainArcs,
    DroneManager*          trainCarTransportDM,
    Train*                 trainMapForBranchArcs,
    Underground*           undergroundForPulseRobots)
{
    // Unlock once the player reaches Rooftop or Underground (normal flow or cheat)
    if (pulseBurstUnlockEligible && !m_unlocked)
        m_unlocked = true;

    if (m_cooldown > 0.f)
        m_cooldown -= static_cast<float>(dt);

    if (!m_unlocked || isGameOver)
        return;
    if (trainMapForBranchArcs
        && trainMapForBranchArcs->IsPlayerInCar2PurplePulseBox(playerCenter, player.GetHitboxSize()))
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
    if (undergroundDM)
    {
        auto arcs = undergroundDM->ApplyDetonation(playerCenter, SKILL_RADIUS, STUN_DURATION);
        allArcs.insert(allArcs.end(), arcs.begin(), arcs.end());
    }
    if (tracerDM)
    {
        auto arcs = tracerDM->ApplyDetonation(playerCenter, SKILL_RADIUS, STUN_DURATION);
        allArcs.insert(allArcs.end(), arcs.begin(), arcs.end());
    }

    // Train 드론: 충격파·VFX는 항상 플레이어 중심(월드). 열차 앵커로 밀리면 이동 중 원이 어긋남.
    if (trainDM)
    {
        auto arcs = trainDM->ApplyDetonation(playerCenter, SKILL_RADIUS, STUN_DURATION);
        allArcs.insert(allArcs.end(), arcs.begin(), arcs.end());
    }
    if (trainSirenDM)
    {
        auto arcs = trainSirenDM->ApplyDetonation(playerCenter, SKILL_RADIUS, STUN_DURATION);
        allArcs.insert(allArcs.end(), arcs.begin(), arcs.end());
    }
    if (trainCarTransportDM)
    {
        auto arcs = trainCarTransportDM->ApplyDetonation(playerCenter, SKILL_RADIUS, STUN_DURATION);
        allArcs.insert(allArcs.end(), arcs.begin(), arcs.end());
    }

    if (extraChainArcs && !extraChainArcs->empty())
        allArcs.insert(allArcs.end(), extraChainArcs->begin(), extraChainArcs->end());

    if (undergroundForPulseRobots)
        undergroundForPulseRobots->ApplyPulseToRobots(playerCenter, SKILL_RADIUS);

    if (trainMapForBranchArcs)
    {
        trainMapForBranchArcs->AppendCarTransportPulseBranchArcs(allArcs, playerCenter, SKILL_RADIUS);
        trainMapForBranchArcs->ApplyPulseToTrainRobots(playerCenter, SKILL_RADIUS);
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

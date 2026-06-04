// Train_Car3.cpp - ThirdTrain (Car3): siren pulse source

#include "Train_Internal.hpp"
#include "DroneManager.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/DebugRenderer.hpp"
#include "MapObjectConfig.hpp"
#include "Robot.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>
namespace
{
constexpr float kCar3InsideFadeHalfSec = 0.22f;
}

// 사이렌 드론 스폰·펄스 주입·추적 로직을 매 프레임 갱신함
// — 주입 완료 시 사이렌을 비활성화하고, 이후 드론 추적 속도를 배율로 줄임
void Train::UpdateCar3Siren(float dt, Player& player, Math::Vec2 playerHbCenter, Math::Vec2 playerHitboxSize,
                            Math::Vec2 mouseWorldPos, bool attackHeld, bool injectGodMode)
{
    if (ShouldHideTrainExteriorHazards())
        return;
    if (!m_car3SirenHbValid || !m_sirenDroneManager)
        return;

    if (m_car3SirenPendingShutdown)
    {
        m_car3SirenPendingShutdown = false;
        m_car3SirenActive          = false;
    }

    constexpr float kInjectPulseTotal  = 5.f;
    constexpr float kInjectDurationSec = 1.5f;
    constexpr float kPostSirenChaseMul = 0.48f; // 사이렌 정지 후 추적 속도 배율

    const float      tl     = MIN_X + m_trainOffset;
    const Math::Vec2 sirenW = { tl + m_car3SirenHb.localCenter.x, MIN_Y + m_car3SirenHb.localCenter.y };

    m_car3SirenWaveAnim += dt * 5.5f;

    const bool inInjectRange =
        Collision::CheckAABB(playerHbCenter, playerHitboxSize, sirenW, { 380.f, 300.f });
    const bool cursorOnSiren = Collision::CheckPointInAABB(mouseWorldPos, sirenW, m_car3SirenHb.size);

    if (m_car3SirenActive)
    {
        const bool playerOnCar3 = (GetPlayerTrainCarIndex(playerHbCenter) == 3);
        if (playerOnCar3)
            m_car3SirenSpawnTimer += static_cast<float>(dt);
        if (playerOnCar3 && m_car3SirenSpawnTimer >= 2.4f && m_sirenDroneManager->GetDrones().size() < 14)
        {
            m_car3SirenSpawnTimer = 0.f;
            const Math::Vec2 spawnPos = { sirenW.x + 25.f, sirenW.y + 55.f };
            Drone& d = m_sirenDroneManager->SpawnDrone(spawnPos, kTrainDroneTexturePath, true);
            ScaleTrainCombatDrone(d);
            d.SetBaseSpeed(185.f);
            d.SetSirenMapDrone(true);
            d.SetTrainCarSegment(3);
        }

        if (attackHeld && inInjectRange && cursorOnSiren)
        {
            const float pulsePerSec    = kInjectPulseTotal / kInjectDurationSec;
            float       pulseThisFrame = pulsePerSec * static_cast<float>(dt);
            if (!injectGodMode)
            {
                pulseThisFrame = std::min(pulseThisFrame, player.GetPulseCore().getPulse().Value());
                if (pulseThisFrame > 0.f)
                    player.GetPulseCore().getPulse().spend(pulseThisFrame);
            }
            if (pulseThisFrame > 0.f || injectGodMode)
            {
                const float deltaT = injectGodMode ? (static_cast<float>(dt) / kInjectDurationSec)
                                                   : (pulseThisFrame / kInjectPulseTotal);
                m_car3SirenInjectT += deltaT;
            }
            if (m_car3SirenInjectT >= 1.f)
            {
                m_car3SirenInjectT           = 1.f;
                m_car3SirenPendingShutdown    = true;
                m_car3SirenSpawnTimer        = 0.f;
            }
        }
    }

    const bool suppressExteriorDrones = ShouldHideTrainExteriorHazards()
        || m_car3InsideTransitionActive || m_car3TunnelInsideTransitionActive;
    const bool hideForSiren =
        IsPlayerHiding(playerHbCenter, playerHitboxSize, player.IsCrouching())
        || IsPlayerInCar2PurplePulseBox(playerHbCenter, playerHitboxSize)
        || suppressExteriorDrones;
    const float chaseMul = m_car3SirenActive ? 1.f : kPostSirenChaseMul;
    const float speedRatio =
        (TRAIN_SPEED > 0.f) ? std::clamp(m_trainCurrentSpeed / TRAIN_SPEED, 0.f, 1.f) : 0.f;
    constexpr float kSirenTracerTrainAssistK = 0.22f;
    const float     trainAssist              = 1.f + kSirenTracerTrainAssistK * speedRatio;
    m_sirenDroneManager->Update(dt, player, playerHitboxSize, hideForSiren, true, chaseMul, trainAssist);
}


// 마우스가 사이렌 위에 있고 플레이어가 근접한 경우 펄스 주입 커서 조건을 반환함
bool Train::IsCar3SirenMouseHoverForPulseInject(Math::Vec2 playerHbCenter, Math::Vec2 playerHbSize,
                                                Math::Vec2 mouseWorld) const
{
    if (!m_car3SirenHbValid || !m_car3SirenActive)
        return false;
    const float      tl     = MIN_X + m_trainOffset;
    const Math::Vec2 sirenW = { tl + m_car3SirenHb.localCenter.x, MIN_Y + m_car3SirenHb.localCenter.y };
    if (!Collision::CheckAABB(playerHbCenter, playerHbSize, sirenW, { 380.f, 300.f }))
        return false;
    return Collision::CheckPointInAABB(mouseWorld, sirenW, m_car3SirenHb.size);
}

bool Train::IsCar3TunnelEnterHovered(Math::Vec2 playerHbCenter, Math::Vec2 playerHbSize, Math::Vec2 mouseWorld) const
{
    if (!m_car3TunnelEnterHbValid || !m_car3InsideViewActive || m_car3InsideTransitionActive
        || m_car3TunnelInsideTransitionActive || m_trainState != TrainState::Stationary)
        return false;
    const float      tl  = MIN_X + m_trainOffset;
    const Math::Vec2 box = { tl + m_car3TunnelEnterHb.localCenter.x, MIN_Y + m_car3TunnelEnterHb.localCenter.y };
    if (!Collision::CheckAABB(playerHbCenter, playerHbSize, box, { 750.f, 550.f }))
        return false;
    const Math::Vec2 cursorHb = { 32.f, 32.f };
    return Collision::CheckPointInAABB(mouseWorld, box, m_car3TunnelEnterHb.size)
           || Collision::CheckAABB(mouseWorld, cursorHb, box, m_car3TunnelEnterHb.size);
}

bool Train::IsCar3ExtensionEnterHovered(Math::Vec2 playerHbCenter, Math::Vec2 playerHbSize, Math::Vec2 mouseWorld) const
{
    if (!m_car3ExtensionEnterHbValid || m_car3InsideTransitionActive)
        return false;
    const float      tl  = MIN_X + m_trainOffset;
    const Math::Vec2 box = { tl + m_car3ExtensionEnterHb.localCenter.x, MIN_Y + m_car3ExtensionEnterHb.localCenter.y };
    if (!Collision::CheckAABB(playerHbCenter, playerHbSize, box, { 560.f, 380.f }))
        return false;
    const Math::Vec2 cursorHb = { 32.f, 32.f };
    return Collision::CheckPointInAABB(mouseWorld, box, m_car3ExtensionEnterHb.size)
           || Collision::CheckAABB(mouseWorld, cursorHb, box, m_car3ExtensionEnterHb.size);
}

bool Train::IsCar3InsideLadderHovered(Math::Vec2 playerHbCenter, Math::Vec2 playerHbSize, Math::Vec2 mouseWorld) const
{
    if (!m_car3InsideLadderHbValid || !m_car3InsideViewActive || m_car3InsideTransitionActive)
        return false;
    const float      tl  = MIN_X + m_trainOffset;
    const Math::Vec2 cursorHb = { 32.f, 32.f };

    auto hoveredLadder = [&](const TrainHitbox& hb, Math::Vec2 nearSize) -> bool
    {
        const Math::Vec2 box = { tl + hb.localCenter.x, MIN_Y + hb.localCenter.y };
        const bool nearEnough = Collision::CheckAABB(playerHbCenter, playerHbSize, box, nearSize);
        if (!nearEnough)
            return false;
        return Collision::CheckPointInAABB(mouseWorld, box, hb.size)
               || Collision::CheckAABB(mouseWorld, cursorHb, box, hb.size);
    };

    if (m_car3InsideOnRoof)
    {
        // 지붕에서 내려갈 때는 SecondInside_2 사다리만 클릭 허용.
        return hoveredLadder(m_car3InsideLadder2Hb, { 360.f, 900.f });
    }

    if (!hoveredLadder(m_car3InsideLadderHb, { 220.f, 320.f }))
        return false;
    return true;
}

void Train::ClimbCar3InsideLadder(Player& player, Math::Vec2 playerHitboxSize)
{
    if (!m_car3InsideLadderHbValid)
        return;

    const float trainWorldLeft = MIN_X + m_trainOffset;
    const float halfH          = playerHitboxSize.y * 0.5f;
    const Math::Vec2 oldHb = player.GetHitboxCenter();

    if (!m_car3InsideOnRoof)
    {
        const float ladderCx = trainWorldLeft + m_car3InsideLadderHb.localCenter.x;
        const float roofTop  = MIN_Y + m_car3InsideRoofHb.localCenter.y + m_car3InsideRoofHb.size.y * 0.5f;
        const Math::Vec2 newHb = { ladderCx, roofTop + halfH };
        m_car3InsideOnRoof = true;
        player.SetCurrentGroundLevel(roofTop);
        player.SetPosition(player.GetPosition() + (newHb - oldHb));
    }
    else
    {
        const float ladder2Cx = trainWorldLeft + m_car3InsideLadder2Hb.localCenter.x;
        const float floorTop  = MIN_Y + m_car3InsideFloorHb.localCenter.y + m_car3InsideFloorHb.size.y * 0.5f;
        // 지붕에서 내려올 때는 항상 SecondInside_2 사다리 앞(해당 X)으로 배치.
        const Math::Vec2 newHb = { ladder2Cx, floorTop + halfH };
        m_car3InsideOnRoof = false;
        player.SetCurrentGroundLevel(floorTop);
        player.SetPosition(player.GetPosition() + (newHb - oldHb));
    }

    player.ResetVelocity();
    player.SetOnGround(true);
}

void Train::DrawCar3InsideFadeOverlay(Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW) const
{
    const bool fadingInside = m_car3InsideTransitionActive;
    const bool fadingTunnel = m_car3TunnelInsideTransitionActive;
    if (!m_skyVAO || (!fadingInside && !fadingTunnel))
        return;
    const float timer = fadingTunnel ? m_car3TunnelInsideTransitionTimer : m_car3InsideTransitionTimer;
    const float t = std::clamp(timer / kCar3InsideFadeHalfSec, 0.f, 2.f);
    const float alpha = (t <= 1.f) ? t : (2.f - t);
    if (alpha <= 0.001f)
        return;
    const float halfW = std::max(viewHalfW, 400.f);
    DrawFilledQuad(colorShader, cameraPos, { halfW * 2.f + 1200.f, HEIGHT + 1800.f }, 0.f, 0.f, 0.f, alpha);
}



// 활성 사이렌의 팽창 파동을 타원 링 여러 개로 그림 (주입 진행도에 따라 투명도 감소함)
void Train::DrawCar3SirenWaves(Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW) const
{
    if (ShouldHideTrainExteriorHazards())
        return;
    if (!m_car3SirenHbValid || !m_car3SirenActive || m_car3SirenInjectT >= 0.995f)
        return;

    const float      tl    = MIN_X + m_trainOffset;
    const Math::Vec2 o = { tl + m_car3SirenHb.localCenter.x, MIN_Y + m_car3SirenHb.localCenter.y + 70.f };

    const float visLeft = cameraPos.x - viewHalfW - 30000.f;
    const float visRight = cameraPos.x + viewHalfW + 30000.f;
    if (o.x < visLeft || o.x > visRight)
        return;

    constexpr int kWaveCount = 17;

    for (int i = 0; i < kWaveCount; ++i)
    {
        const float progress = std::fmod(
            m_car3SirenWaveAnim * 0.010f + static_cast<float>(i) * (1.0f / static_cast<float>(kWaveCount)),
            1.0f
        );

        const float minRad = 55.f;
        const float maxRad = 9000.f;

        const float easeOut = 1.f - (1.f - progress) * (1.f - progress); 
        const float easedProgress = progress * 0.90f + easeOut * 0.10f;
        const float rad = minRad + (maxRad - minRad) * easedProgress;

        const float fadeStart = 0.85f;
        const float fadeProgress = std::clamp(
            (progress - fadeStart) / (1.f - fadeStart),
            0.f,
            1.f
        );

        const float slowFade = 1.f - fadeProgress * 0.25f;

        const float alpha =
            0.55f * slowFade * (1.f - m_car3SirenInjectT);

        const float widthMul = 4.0f;
        const float heightMul = 2.0f + easedProgress * 1.4f;

        const float maxThickness = 5.0f;
        const float minThickness = 0.5f;
        const float waveThickness =
            maxThickness + (minThickness - maxThickness) * easedProgress;

        DrawCircleLine(
            colorShader,
            o,
            { rad * widthMul, rad * heightMul },
            waveThickness,
            1.0f, 0.08f, 0.12f,
            alpha
        );
    }
}



// 사이렌 펄스 차단 진행도를 사이렌 옆에 세로 게이지로 그림 (0→1 채움)
void Train::DrawCar3SirenProgressGauge(Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW) const
{
    if (ShouldHideTrainExteriorHazards())
        return;
    if (!m_car3SirenHbValid || !m_car3SirenActive || !m_skyVAO)
        return;

    const float      tl     = MIN_X + m_trainOffset;
    const Math::Vec2 posC = { tl + m_car3SirenHb.localCenter.x, MIN_Y + m_car3SirenHb.localCenter.y };
    const Math::Vec2 half = m_car3SirenHb.size * 0.5f;

    // PulseSource::DrawRemainGauge 와 동일한 배치(오브젝트 오른쪽 세로 바)
    const float barW            = 10.0f;
    const float barH            = std::min(110.0f, std::max(44.0f, m_car3SirenHb.size.y * 0.9f));
    const float padY            = 10.0f;
    const float horizNudgeRight = 38.0f;
    const float vertNudgeDown   = -8.0f;

    const float rx = posC.x + half.x;
    const float ty = posC.y + half.y;
    const float barCenterX = rx - 10.0f - barW * 0.5f + horizNudgeRight;
    const float innerTopY    = ty + padY + vertNudgeDown;
    const float innerCenterY = innerTopY - barH * 0.5f;

    const float visLeft  = cameraPos.x - viewHalfW - 400.f;
    const float visRight = cameraPos.x + viewHalfW + 400.f;
    if (barCenterX < visLeft || barCenterX > visRight)
        return;

    colorShader.use();

    const Math::Vec2 bgSize = { barW + 6.0f, barH + 6.0f };
    DrawFilledQuad(colorShader, { barCenterX, innerCenterY }, bgSize, 0.2f, 0.2f, 0.2f, 1.0f);

    DrawFilledQuad(colorShader, { barCenterX, innerCenterY }, { barW, barH }, 0.14f, 0.14f, 0.18f, 1.0f);

    const float t      = std::clamp(m_car3SirenInjectT, 0.f, 1.f);
    const float fillH  = barH * t;
    if (fillH > 0.5f)
    {
        const float innerBottomY = innerTopY - barH;
        const float fillCenterY  = innerBottomY + fillH * 0.5f;
        DrawFilledQuad(colorShader, { barCenterX, fillCenterY }, { barW, fillH }, 0.8f, 0.2f, 1.0f, 1.0f);
    }
}


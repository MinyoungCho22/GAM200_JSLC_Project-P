// Train_Update.cpp - IsPlayerHiding, main Update loop, entry timers

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

// 플레이어가 히딩 스팟 위에서 웅크리고 있으면 true를 반환함 (드론 탐지 차단 조건)
bool Train::IsPlayerHiding(Math::Vec2 playerHbCenter, Math::Vec2 playerHitboxSize, bool isPlayerCrouching) const
{
    if (!isPlayerCrouching) return false;

    const float trainWorldLeft = MIN_X + m_trainOffset;
    // 스프라이트/히트박스와 JSON 박스 미세 오차 허용
    constexpr float kHidingMargin = 36.f;
    for (const auto& spot : m_hidingSpots)
    {
        Math::Vec2 worldPos = { trainWorldLeft + spot.localCenter.x, MIN_Y + spot.localCenter.y };
        Math::Vec2 detectSize = { spot.size.x + kHidingMargin * 2.f, spot.size.y + kHidingMargin * 2.f };
        if (Collision::CheckAABB(playerHbCenter, playerHitboxSize, worldPos, detectSize))
            return true;
    }
    return false;
}


// ---------------------------------------------------------------------------
// ResolveJumpThroughAABB – thin “pipe” tiers on Third_ThirdTrain: pass upward, land from above, crouch = fall through.
// ---------------------------------------------------------------------------
// 파이프 점프-스루 발판: 아래→위 통과 허용, 위에서 착지 가능, 웅크리면 낙하함
static bool ResolveJumpThroughAABB(Player& player, Math::Vec2& currentHbCenter,
                                   const Math::Vec2& playerHalfSize,
                                   const Math::Vec2& obsCenter, const Math::Vec2& obsSize,
                                   bool crouchHeld)
{
    Math::Vec2 obsHalf = obsSize * 0.5f;
    Math::Vec2 obsMin  = obsCenter - obsHalf;
    Math::Vec2 obsMax  = obsCenter + obsHalf;
    Math::Vec2 pMin    = currentHbCenter - playerHalfSize;
    Math::Vec2 pMax    = currentHbCenter + playerHalfSize;

    // Jump-through slab은 가로 구간에서만 고려.
    const bool horizOverlap = pMax.x > obsMin.x && pMin.x < obsMax.x;
    if (!horizOverlap)
        return false;

    if (crouchHeld)
        return false;

    const float platTop = obsMax.y;
    const float vy      = player.GetVelocity().y;
    const float feet    = currentHbCenter.y - playerHalfSize.y;

    // 이 발판보다 확실히 위층에 있으면 무시 (3층 위에서 2층/바닥 발판 간섭 차단).
    constexpr float kTierAboveSlab = 36.0f;
    if (feet > platTop + kTierAboveSlab)
        return false;

    // 빠르게 상승 중이면 무조건 통과.
    // (2층 파이프에서 점프 시작 프레임에 같은 파이프가 다시 스냅해 점프를 막는 문제 방지)
    constexpr float kRiseVy       = 52.0f;
    if (vy > kRiseVy)
        return false;

    // 핵심: 정확히 닿는 프레임(겹침 0)도 착지로 유지하도록 작은 상향 오차를 허용.
    constexpr float kSnapDownRange = 96.0f;
    constexpr float kSnapUpRange   = 8.0f;
    if (feet < platTop - kSnapDownRange || feet > platTop + kSnapUpRange)
        return false;

    Math::Vec2 newHbCenter = currentHbCenter;
    newHbCenter.y = platTop + playerHalfSize.y;
    Math::Vec2 shift = newHbCenter - currentHbCenter;
    player.SetPosition(player.GetPosition() + shift);
    currentHbCenter = newHbCenter;
    player.SetOnGround(true);
    return true;
}

// Maintain stable standing when the player is exactly touching a platform top.
// Without this, exact-contact frames may flip on/off ground and cause jitter.
// 발판 위에 정확히 닿는 프레임에서도 착지 상태를 유지하도록 스냅 보정을 적용함
static bool SnapToTopSupport(Player& player, Math::Vec2& currentHbCenter,
                             const Math::Vec2& playerHalfSize,
                             const Math::Vec2& obsCenter, const Math::Vec2& obsSize,
                             Train::TrainHitboxKind kind, bool crouchHeld)
{
    if (kind == Train::TrainHitboxKind::JumpThroughPipe && crouchHeld)
        return false;

    Math::Vec2 obsHalf = obsSize * 0.5f;
    Math::Vec2 obsMin  = obsCenter - obsHalf;
    Math::Vec2 obsMax  = obsCenter + obsHalf;

    const float pMinX = currentHbCenter.x - playerHalfSize.x;
    const float pMaxX = currentHbCenter.x + playerHalfSize.x;
    const bool horizOverlap = pMaxX > obsMin.x && pMinX < obsMax.x;
    if (!horizOverlap)
        return false;

    const float platTop = obsMax.y;
    const float feet    = currentHbCenter.y - playerHalfSize.y;
    const float vy      = player.GetVelocity().y;

    // Don't glue while strongly rising.
    if (vy > 60.0f)
        return false;

    const float aboveTol = (kind == Train::TrainHitboxKind::JumpThroughPipe) ? 10.0f : 8.0f;
    // 아래에서 점프해 덱에 올라오기 쉽게(떨어진 뒤 재탑승).
    const float belowTol = (kind == Train::TrainHitboxKind::JumpThroughPipe) ? 72.0f : 88.0f;

    if (feet > platTop + aboveTol || feet < platTop - belowTol)
        return false;

    Math::Vec2 newHbCenter = currentHbCenter;
    newHbCenter.y = platTop + playerHalfSize.y;
    Math::Vec2 shift = newHbCenter - currentHbCenter;
    player.SetPosition(player.GetPosition() + shift);
    currentHbCenter = newHbCenter;
    player.SetOnGround(true);
    return true;
}


// ---------------------------------------------------------------------------
// ResolveAABB – identical algorithm to Underground so behaviour is consistent.
//   Uses actual penetration depth (min-of-max minus max-of-min) on each axis,
//   resolves along the axis with the SMALLER overlap.
//   Returns true if the player was pushed UP onto the surface (standing on top).
// ---------------------------------------------------------------------------
// 슬랩이 가로로 긴 얇은 판인지 확인함 (발판 충돌 특수 처리 대상 여부 판별용)
static bool IsThinHorizontalSlab(const Math::Vec2& obsSize)
{
    return IsThinHorizontalTrainSlab(obsSize);
}

// Underground와 동일한 알고리즘으로 플레이어-장애물 AABB를 해소함
// — 겹침이 작은 축으로 밀어내며, 위로 밀린 경우(발판 착지) true를 반환함
static bool ResolveAABB(Player& player, Math::Vec2& currentHbCenter,
                         const Math::Vec2& playerHalfSize,
                         const Math::Vec2& obsCenter, const Math::Vec2& obsSize)
{
    if (!Collision::CheckAABB(currentHbCenter, playerHalfSize * 2.0f, obsCenter, obsSize))
        return false;

    Math::Vec2 obsHalf = obsSize * 0.5f;
    Math::Vec2 obsMin  = obsCenter - obsHalf;
    Math::Vec2 obsMax  = obsCenter + obsHalf;
    Math::Vec2 pMin    = currentHbCenter - playerHalfSize;
    Math::Vec2 pMax    = currentHbCenter + playerHalfSize;

    // 얇은 가로 발판: overlapY가 작을 때 overlapX < overlapY 로 가로 밀림이 나와 웅크리기 낙하 시 옆으로 튐 → 세로 착지만
    if (IsThinHorizontalSlab(obsSize))
    {
        const float platTop = obsMax.y;
        const float platBot = obsMin.y;
        const float feet    = currentHbCenter.y - playerHalfSize.y;
        const float vy      = player.GetVelocity().y;

        // 위층에 있을 때 아래쪽 얇은 발판(열차 바닥 등)과 몸만 겹치면 해석하지 않음
        constexpr float kTierAboveSlab = 36.0f;
        if (feet > platTop + kTierAboveSlab)
            return false;

        const bool horizOverlap = pMax.x > obsMin.x && pMin.x < obsMax.x;
        // 덱 가로 전체: 플레이어 박스가 닿기만 하면 착지(시안 히트박스와 동일, 끝에서 떨어지는 오판 방지).
        if (horizOverlap && vy <= 260.0f && feet <= platTop + 64.0f && feet >= platTop - 168.0f)
        {
            Math::Vec2 newHbCenter = currentHbCenter;
            newHbCenter.y          = platTop + playerHalfSize.y;
            Math::Vec2 shift       = newHbCenter - currentHbCenter;
            player.SetPosition(player.GetPosition() + shift);
            currentHbCenter = newHbCenter;
            player.SetOnGround(true);
            return true;
        }
    }

    float overlapX = std::min(pMax.x, obsMax.x) - std::max(pMin.x, obsMin.x);
    float overlapY = std::min(pMax.y, obsMax.y) - std::max(pMin.y, obsMin.y);

    Math::Vec2 newHbCenter = currentHbCenter;
    bool landedOnTop = false;

    if (overlapX < overlapY)
    {
        // Horizontal push
        if (currentHbCenter.x < obsCenter.x)
            newHbCenter.x = obsMin.x - playerHalfSize.x;
        else
            newHbCenter.x = obsMax.x + playerHalfSize.x;
    }
    else
    {
        if (currentHbCenter.y < obsCenter.y)
        {
            // Player is below the obstacle – hit ceiling from underneath
            newHbCenter.y = obsMin.y - playerHalfSize.y;
            player.ResetVelocity();
        }
        else
        {
            // Player is above the obstacle – land on top surface
            newHbCenter.y = obsMax.y + playerHalfSize.y;
            // Only zero Y velocity (preserves X so the player can walk freely)
            player.SetOnGround(true);
            landedOnTop = true;
        }
    }

    Math::Vec2 shift = newHbCenter - currentHbCenter;
    player.SetPosition(player.GetPosition() + shift);
    currentHbCenter = newHbCenter;
    return landedOnTop;
}


// ---------------------------------------------------------------------------
// 열차 맵 전체 상태를 매 프레임 갱신함
// — 열차 이동·충돌·플레이어 반응·드론·로봇·칸별 로직을 순차 처리함
// ---------------------------------------------------------------------------
void Train::Update(double dt, Player& player, Math::Vec2 playerHitboxSize,
                   bool pulseAbsorbHeld, bool carTransportInjectHeld, bool ignoreCarInjectPulseCost,
                   bool attackHeld, bool attackTriggered, Math::Vec2 mouseWorldPos,
                   int carTransportInjectForcedSlot)
{
    const float fdt = static_cast<float>(dt);
    m_encounterScriptTime += fdt;

    Math::Vec2 playerPos = player.GetPosition();

    // Only run when player is roughly within (or near) the train region.
    // Keep the Y band generous: on upper containers/pipes, a forward jump can briefly put
    // the player's draw-center above the old bound. Returning here freezes train actors.
    constexpr float kTrainUpdateLowerMarginY = 900.0f;
    constexpr float kTrainUpdateUpperMarginY = 1800.0f;
    constexpr float kTrainUpdateLeftMarginX  = 200.0f;
    const bool playerInSubway =
        playerPos.y >= MIN_Y - kTrainUpdateLowerMarginY
        && playerPos.y <= MIN_Y + HEIGHT + kTrainUpdateUpperMarginY
        && playerPos.x >= MIN_X - kTrainUpdateLeftMarginX;

    if (!playerInSubway) return;

    {
        const float dTrain = m_trainOffset - m_prevTrainOffsetActors;
        ApplyTrainMotionToDronesAndRobots(dTrain);
        m_prevTrainOffsetActors = m_trainOffset;
    }

    // Valve clockwise drag interaction (Car5 top valve) -> tank spill VFX.
    {
        const Math::Vec2 valveWorld = { MIN_X + m_trainOffset + m_valveLocalCenter.x, MIN_Y + m_valveLocalCenter.y };
        const bool canHover = IsValveMouseHoverable(player.GetHitboxCenter(), playerHitboxSize, mouseWorldPos);

        if (!attackHeld)
            m_valveDragging = false;

        if (attackTriggered && canHover)
        {
            const Math::Vec2 d = mouseWorldPos - valveWorld;
            m_valvePrevMouseAngle = std::atan2(d.y, d.x);
            m_valveDragging = true;
        }

        if (m_valveDragging && attackHeld)
        {
            const Math::Vec2 d = mouseWorldPos - valveWorld;
            const float currA = std::atan2(d.y, d.x);
            const float delta = WrapAnglePi(currA - m_valvePrevMouseAngle);
            m_valvePrevMouseAngle = currA;

            // Clockwise in world space = negative angular delta.
            if (delta < 0.0f)
                m_valveOpenAccum = std::min(3.5f, m_valveOpenAccum + (-delta) * 1.2f);
        }
        else
            m_valveOpenAccum = std::max(0.0f, m_valveOpenAccum - fdt * 0.45f);

        m_valveOpenT = std::clamp(m_valveOpenAccum / 2.8f, 0.0f, 1.0f);
        const float pressureFollow = std::min(1.0f, fdt * 1.25f);
        m_valvePressureT += (m_valveOpenT - m_valvePressureT) * pressureFollow;
        if (m_valvePressureT > 0.01f)
            m_valveWaterAnimTime += fdt * (0.8f + m_valvePressureT * 2.7f);
    }
    UpdateValveWaterParticles(fdt);

    TryActivateCar5Encounter(player.GetHitboxCenter(), playerHitboxSize);

    UpdateCar2PurpleContainer(fdt, player, mouseWorldPos, attackTriggered, pulseAbsorbHeld, ignoreCarInjectPulseCost);
    UpdateCar3Siren(fdt, player, player.GetHitboxCenter(), playerHitboxSize, mouseWorldPos, attackHeld,
                    ignoreCarInjectPulseCost);

    // --- Drone / Robot: FourthTrain 스크립트 (Car5 발판 진입 전까지 정지) ---
    const bool isPlayerHiding = IsPlayerHiding(player.GetHitboxCenter(), playerHitboxSize, player.IsCrouching());
    const bool inCar2PulseBox = IsPlayerInCar2PurplePulseBox(player.GetHitboxCenter(), playerHitboxSize);
    const bool trainEnemyUndetect = isPlayerHiding || inCar2PulseBox;
    player.SetHiding(isPlayerHiding);
    player.SetTrainEnemyUndetectable(trainEnemyUndetect);

    UpdateCarTransportDrones(fdt, player, playerHitboxSize, trainEnemyUndetect);

    UpdateTrainDeckPatrolRobots(fdt, player, player.GetHitboxCenter(), playerHitboxSize);

    if (m_droneManager)
        m_droneManager->Update(fdt, player, playerHitboxSize, trainEnemyUndetect, true, 1.f);

    if (m_car5EncounterActive)
        UpdateTrainEncounterScript(fdt, player);
    else if (m_droneManager)
    {
        // 인카운터 전: 1·2·3호차 전투 드론은 Drone::Update만 사용(칸 간 추적·Q 넉백). 4호차 저공 호버, 5호차 성형.
        auto&          drones = m_droneManager->GetDrones();
        const float    baseY = Train::MIN_Y + 95.f + 82.f;
        const float    tl    = MIN_X + m_trainOffset;
        const float    c5L   = tl + m_car1Width + m_car2Width + m_car3Width + m_car4Width;
        const float    c5R   = c5L + m_car5Width;
        const Math::Vec2 pTarget = player.GetHitboxCenter();
        /// 히딩 중: 플레이어 X 추적 대신 칸 안에서 좌우 왕복(레이더 흔들림 + 배회)
        const auto trackXInRange = [&](float left, float right, float wanderFreq, size_t idx) -> float
        {
            if (!trainEnemyUndetect)
                return std::clamp(pTarget.x, left, right);
            const float mid  = 0.5f * (left + right);
            const float half = 0.5f * (right - left) - 40.f;
            if (half <= 8.f)
                return mid;
            return mid + std::sin(m_encounterScriptTime * wanderFreq + static_cast<float>(idx) * 0.91f) * half * 0.88f;
        };
        const float car12DroneHoverBase =
            m_car3SirenHbValid ? (MIN_Y + m_car3SirenHb.localCenter.y + 55.f)
                               : (Train::MIN_Y + kTrainFlatbedDeckTopLocalY + 380.f);

        for (size_t i = 0; i < drones.size(); ++i)
        {
            auto& d = drones[i];
            if (d.IsDead())
                continue;
            if (d.IsHit() || d.IsStunned())
                continue;
            const int seg = d.GetTrainCarSegment();
            if (seg == 4)
            {
                Math::Vec2 p = d.GetPosition();
                p.y          = baseY + std::sin(m_encounterScriptTime * 2.1f + static_cast<float>(i) * 0.78f) * 20.f;
                d.SetPosition(p);
                d.SetVelocity({ 0.f, 0.f });
                d.SetBaseSpeed(95.f);
                continue;
            }
            if (seg == 5)
            {
                const float ySiren =
                    m_car3SirenHbValid ? (MIN_Y + m_car3SirenHb.localCenter.y + 55.f) : car12DroneHoverBase;
                const float hoverY =
                    ySiren + std::sin(m_encounterScriptTime * 2.2f + static_cast<float>(i) * 0.61f) * 5.f;
                Math::Vec2 p = d.GetPosition();
                const float desiredX = trackXInRange(c5L + 100.f, c5R - 100.f, 2.95f, i);
                const float ap       = 118.f + static_cast<float>(i % 5) * 11.f;
                p.x                  = std::clamp(
                    p.x + std::clamp(desiredX - p.x, -ap * fdt, ap * fdt),
                    c5L + 100.f, c5R - 100.f);
                p.y = hoverY;
                d.SetPosition(p);
                d.SetVelocity({ 0.f, 0.f });
                d.SetBaseSpeed(88.f + static_cast<float>(i % 6) * 5.5f);
                continue;
            }
        }
    }

    ApplyValveWaterDamageToEnemies(fdt);

    // Use hitbox center (matches Underground / other maps)
    Math::Vec2 currentHbCenter = player.GetHitboxCenter();
    const Math::Vec2 playerHalfSize = playerHitboxSize * 0.5f;

    // --- Config obstacle collision (static) ---
    for (const auto& obs : m_obstacles)
        ResolveAABB(player, currentHbCenter, playerHalfSize, obs.pos, obs.size);

    // --- Train hitbox collision ---
    const float trainWorldLeft = MIN_X + m_trainOffset;
    bool playerOnTrainSurface = false;
    bool playerOnStaticRail    = false;
    bool playerOnJumpThroughSurface = false;

    const bool crouchHeld = player.IsCrouching();
    const bool crouchPressed = crouchHeld && !m_prevCrouchHeld;

    // ─── 파이프 드롭스루 (S 누르는 순간) ───
    // collision pass 전에 직접 검사: 파이프 top과 발 차이가 작으면 즉시 -100px nudge.
    // 이렇게 하면 SnapToTopSupport / ResolveJumpThroughAABB의 race 조건과 무관하게 확실히 드롭된다.
    if (m_pipeDropCooldown > 0.0f)
        m_pipeDropCooldown = std::max(0.0f, m_pipeDropCooldown - fdt);

    if (crouchPressed && m_pipeDropCooldown <= 0.0f)
    {
        const Math::Vec2 hc = player.GetHitboxCenter();
        const float feet    = hc.y - playerHalfSize.y;
        for (const auto& hb : m_trainHitboxes)
        {
            if (!hb.collision) continue;
            if (hb.kind != Train::TrainHitboxKind::JumpThroughPipe) continue;

            const float xMin = trainWorldLeft + hb.localCenter.x - hb.size.x * 0.5f;
            const float xMax = trainWorldLeft + hb.localCenter.x + hb.size.x * 0.5f;
            const float top  = MIN_Y          + hb.localCenter.y + hb.size.y * 0.5f;

            const bool xOverlap = (hc.x + playerHalfSize.x > xMin) && (hc.x - playerHalfSize.x < xMax);
            if (!xOverlap) continue;
            if (std::abs(feet - top) > 24.0f) continue; // 파이프 top 근처에 발이 있어야 함

            constexpr float kPipeDropNudge = 100.0f;
            player.SetPosition(player.GetPosition() + Math::Vec2{ 0.0f, -kPipeDropNudge });
            player.SetOnGround(false);
            m_pipeDropCooldown = 0.18f;
            m_crouchCarryLatched = true;
            currentHbCenter = player.GetHitboxCenter();
            break;
        }
    }

    if (player.IsGodMode() || player.IsDead())
        m_trainCarGapFalling = false;

    bool skipTrainDeckPhysics = false;

    const bool gapTrigger = PlayerStandsInTrainCarGap(trainWorldLeft, m_totalTrainWidth, currentHbCenter,
                                                      playerHalfSize, m_trainHitboxes, player);
    if (gapTrigger && !m_trainCarGapFalling)
    {
        m_trainCarGapFalling   = true;
        m_airborneFromTrain    = false;
        player.SetOnGround(false);
        const float vy = player.GetVelocity().y;
        if (vy > -200.f)
            player.SetVerticalVelocity(std::min(vy - 90.f, -400.f));
        Logger::Instance().Log(Logger::Severity::Info, "Train: player slipping through car gap — falling to rail.");
    }

    if (m_trainCarGapFalling)
    {
        skipTrainDeckPhysics = true;
        player.SetOnGround(false);
    }

    if (!skipTrainDeckPhysics)
    {
        for (const auto& hb : m_trainHitboxes)
        {
            if (!hb.collision)
                continue;
            if (IsCar2PurpleHitbox(hb) && m_car2HidePhase != Car2HidePhase::None)
                continue;

            Math::Vec2 hbWorld = {
                trainWorldLeft + hb.localCenter.x,
                MIN_Y          + hb.localCenter.y
            };

            bool landed = false;
            if (hb.kind == Train::TrainHitboxKind::JumpThroughPipe)
            {
                if (m_pipeDropCooldown > 0.0f) continue; // 드롭 직후엔 파이프 다시 잡지 않음
                landed = ResolveJumpThroughAABB(player, currentHbCenter, playerHalfSize, hbWorld, hb.size, crouchHeld);
            }
            else
                landed = ResolveAABB(player, currentHbCenter, playerHalfSize, hbWorld, hb.size);

            if (landed)
            {
                playerOnTrainSurface = true;
                if (hb.kind == Train::TrainHitboxKind::JumpThroughPipe)
                    playerOnJumpThroughSurface = true;
            }
        }

        // Exact-touch stabilization pass:
        // if collision solver didn't mark landed this frame, keep support when feet are already on a top surface.
        if (!playerOnTrainSurface)
        {
            for (const auto& hb : m_trainHitboxes)
            {
                if (!hb.collision)
                    continue;
                if (IsCar2PurpleHitbox(hb) && m_car2HidePhase != Car2HidePhase::None)
                    continue;

                Math::Vec2 hbWorld = {
                    trainWorldLeft + hb.localCenter.x,
                    MIN_Y          + hb.localCenter.y
                };
                if (hb.kind == Train::TrainHitboxKind::JumpThroughPipe && m_pipeDropCooldown > 0.0f)
                    continue;
                if (SnapToTopSupport(player, currentHbCenter, playerHalfSize, hbWorld, hb.size, hb.kind, crouchHeld))
                {
                    playerOnTrainSurface = true;
                    if (hb.kind == Train::TrainHitboxKind::JumpThroughPipe)
                        playerOnJumpThroughSurface = true;
                    break;
                }
            }
        }
    }

    // --- 월드 고정 레일 발판 (rail.png, 열차 이동과 무관) ---
    for (const auto& hb : m_staticWorldHitboxes)
    {
        if (!hb.collision)
            continue;
        const Math::Vec2 hbWorld = { MIN_X + hb.localCenter.x, MIN_Y + hb.localCenter.y };
        const bool       landed  = ResolveAABB(player, currentHbCenter, playerHalfSize, hbWorld, hb.size);
        if (landed)
            playerOnStaticRail = true;
    }
    if (!playerOnStaticRail)
    {
        for (const auto& hb : m_staticWorldHitboxes)
        {
            if (!hb.collision)
                continue;
            const Math::Vec2 hbWorld = { MIN_X + hb.localCenter.x, MIN_Y + hb.localCenter.y };
            if (SnapToTopSupport(player, currentHbCenter, playerHalfSize, hbWorld, hb.size,
                                 Train::TrainHitboxKind::Solid, crouchHeld))
            {
                playerOnStaticRail = true;
                break;
            }
        }
    }

    // 갭 낙하 후 레일 착지: 그때 펄스 전부 소모(기차에 치인 연출).
    if (m_trainCarGapFalling && playerOnStaticRail && !player.IsGodMode() && !player.IsDead())
    {
        auto& pulse = player.GetPulseCore().getPulse();
        pulse.spend(pulse.Value() + 1.0f);
        m_trainCarGapFalling = false;
        Logger::Instance().Log(Logger::Severity::Info, "Train: car gap — landed on rail, pulse depleted.");
    }

    // 드롭은 위쪽에서 이미 처리됨. 여기서는 prev 상태만 갱신.
    m_prevCrouchHeld = crouchHeld;

    // Track whether player is on train this frame
    m_playerOnTrain = playerOnTrainSurface;

    // 발판에서 벗어났으면 지면 플래그 해제 — 단 정적 레일 위에서는 점프 가능하도록 유지
    if (!playerOnTrainSurface && !playerOnStaticRail)
        player.SetOnGround(false);

    UpdateCarTransport(fdt, player, currentHbCenter, carTransportInjectHeld, ignoreCarInjectPulseCost,
                       carTransportInjectForcedSlot);

    // 점프 직후 공중이어도 캐리 밴드 안이면 열차 속도에 맞춰 같이 이동(지면처럼 느껴지게 함)
    {
        const Math::Vec2 hcL  = player.GetHitboxCenter();
        const Math::Vec2 hhL  = playerHitboxSize * 0.5f;
        const float     tlNow = MIN_X + m_trainOffset;
        const bool      inBand = HitboxInTrainCarryBand(tlNow, m_totalTrainWidth, hcL, hhL);
        if (playerOnTrainSurface)
            m_airborneFromTrain = false;
        else if (!player.IsOnGround() && inBand && (m_prevPlayerOnTrain || m_airborneFromTrain))
            m_airborneFromTrain = true;
        else         if (!inBand || (player.IsOnGround() && !playerOnTrainSurface))
            m_airborneFromTrain = false;
        if (m_trainCarGapFalling)
            m_airborneFromTrain = false;
    }

    // --- Entry countdown → automatic departure ---
    if (m_entryTimer >= 0.0f && m_trainState == TrainState::Stationary)
    {
        m_entryTimer += fdt;
        if (m_entryTimer >= TRAIN_DEPART_DELAY)
        {
            m_trainState     = TrainState::Moving;
            m_trainCurrentSpeed = 0.0f;
            m_departedMsgTimer = 2.0f; // show "Train is moving!" for 2 seconds
            m_trainStartSound.Play();
            m_trainRunLoopSound.Play();
            m_trainRunLoopSound.SetVolume(0.08f); // start quietly at departure
            RequestTrainCameraShake(6.f);
            Logger::Instance().Log(Logger::Severity::Info, "Train: Train is now moving!");
        }
    }

    // Tick the "departed" flash message
    if (m_departedMsgTimer > 0.0f)
        m_departedMsgTimer -= fdt;

    if (m_car5ValveHintTimer > 0.0f)
        m_car5ValveHintTimer -= fdt;

    // --- Train movement ---
    if (m_trainState == TrainState::Moving)
    {
        m_trainCurrentSpeed = std::min(TRAIN_SPEED, m_trainCurrentSpeed + TRAIN_ACCEL * fdt);
        const float move = m_trainCurrentSpeed * fdt;
        m_trainOffset += move;

        // Running loop volume scales with speed ratio.
        // Quiet right after departure, louder as acceleration approaches max speed.
        const float speedRatio = (TRAIN_SPEED > 0.0f) ? (m_trainCurrentSpeed / TRAIN_SPEED) : 1.0f;
        const float runVol = 0.08f + speedRatio * 0.52f; // 0.08 -> 0.60
        m_trainRunLoopSound.SetVolume(runVol);

        const Math::Vec2 hc = player.GetHitboxCenter();
        const Math::Vec2 hh = playerHitboxSize * 0.5f;
        const float twLeft = MIN_X + m_trainOffset - move;
        const bool inCarryBand = HitboxInTrainCarryBand(twLeft, m_totalTrainWidth, hc, hh);

        if ((m_playerOnTrain || m_airborneFromTrain || m_trainCarGapFalling) && inCarryBand)
            player.SetPosition(player.GetPosition() + Math::Vec2{ move, 0.0f });
    }

    // --- Train combat progression: 클리어 전에는 다음 칸 경계에서 막음, 끝 칸에서는 오른쪽 낙사 방지 ---
    currentHbCenter = player.GetHitboxCenter();
    {
        const float capWorldX = GetTrainCombatAdvanceCapWorldX();
        if (currentHbCenter.x > capWorldX)
        {
            const float dx = capWorldX - currentHbCenter.x;
            Math::Vec2 pp = player.GetPosition();
            player.SetPosition({ pp.x + dx, pp.y });
            currentHbCenter = player.GetHitboxCenter();
        }
    }

    // --- Left map boundary ---
    currentHbCenter = player.GetHitboxCenter();
    if (currentHbCenter.x - playerHalfSize.x < MIN_X)
        player.SetPosition({ player.GetPosition().x + (MIN_X - (currentHbCenter.x - playerHalfSize.x)), player.GetPosition().y });

    m_prevPlayerOnTrain = playerOnTrainSurface;
    m_prevOnJumpThroughSurface = playerOnJumpThroughSurface;
}

// 플레이어가 열차에 처음 탑승했을 때 출발 카운트다운을 시작하고 초기 상태를 리셋함
void Train::StartEntryTimer()
{
    if (m_entryTimer < 0.0f) // only start once
    {
        m_entryTimer  = 0.0f;
        m_trainState  = TrainState::Stationary;
        m_trainOffset = 0.0f;
        m_trainCurrentSpeed = 0.0f;
        m_prevPlayerOnTrain  = false;
        m_airborneFromTrain  = false;
        m_crouchCarryLatched = false;
        m_prevOnJumpThroughSurface = false;
        m_prevCrouchHeld = false;
        m_pipeDropCooldown = 0.0f;
        m_trainCarGapFalling = false;
        m_trainStartSound.Stop();
        m_trainRunLoopSound.Stop();
        m_valveDragging = false;
        m_valveOpenAccum = 0.0f;
        m_valveOpenT = 0.0f;
        m_valvePressureT = 0.0f;
        m_valveWaterAnimTime = 0.0f;
        m_valveParticleSpawnCarry = 0.0f;
        m_valveParticleCounter = 0;
        m_valveWaterParticles.clear();
        m_car5EncounterActive       = false;
        m_car5ValveHintTimer        = 0.0f;
        m_encounterScriptTime     = 0.f;
        m_trainDeckRobotWasAirborne.assign(m_robots.size(), false);
        m_trainDeckRobotJumpPrepT.assign(m_robots.size(), 0.f);
        m_trainDeckRobotUsedLandingShake.assign(m_robots.size(), false);
        m_pendingTrainCameraShakePx = 0.f;
        m_prevTrainOffsetActors   = m_trainOffset;
        if (!m_droneWaterCd.empty())
            std::fill(m_droneWaterCd.begin(), m_droneWaterCd.end(), 0.f);
        if (!m_carTransportDroneWaterCd.empty())
            std::fill(m_carTransportDroneWaterCd.begin(), m_carTransportDroneWaterCd.end(), 0.f);
        m_car2HidePhase        = Car2HidePhase::None;
        m_car2HideSeqTime      = 0.f;
        m_car2InsideCharge     = 0.f;
        m_car2InsideLockTimer  = 0.f;
        m_car2ReEnterCooldown  = 0.f;
        m_car2EnterSavedValid  = false;
        m_car3SirenActive     = true;
        m_car3SirenInjectT    = 0.f;
        m_car3SirenSpawnTimer = 0.f;
        m_car3SirenPendingShutdown = false;
        if (m_sirenDroneManager)
            m_sirenDroneManager->ClearAllDrones();
        ResetCarTransportSlotsToInitialState();
        Logger::Instance().Log(Logger::Severity::Info,
            "Train: Entry timer started – train departs in %.1f s", TRAIN_DEPART_DELAY);
        m_trainCheatCarUnlock      = false;
    }
}

// 체크포인트 부활 등에서 타이머를 초기화한 뒤 재시작함
void Train::RestartEntryTimer()
{
    // Drop guard so checkpoint respawn can restore true initial-train state.
    m_entryTimer = -1.0f;
    StartEntryTimer();
}


// ---------------------------------------------------------------------------
// 출발 카운트다운 남은 시간 또는 출발 메시지 텍스트를 반환함 (해당 없으면 빈 문자열 반환함)
// ---------------------------------------------------------------------------
std::string Train::GetDepartureAnnouncementText() const
{
    if (m_trainState == TrainState::Moving)
    {
        if (m_departedMsgTimer > 0.0f)
            return "The train is now moving!";
        return "";
    }

    if (m_entryTimer >= 0.0f && m_entryTimer < TRAIN_DEPART_DELAY)
    {
        const int secsLeft =
            std::clamp(static_cast<int>(std::ceil(TRAIN_DEPART_DELAY - m_entryTimer)), 1, 3);
        return "Departure in " + std::to_string(secsLeft) + (secsLeft == 1 ? " second." : " seconds.");
    }

    return "";
}


// ---------------------------------------------------------------------------
// DrawFilledQuad
// ---------------------------------------------------------------------------
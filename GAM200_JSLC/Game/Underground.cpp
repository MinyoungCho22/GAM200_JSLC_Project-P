//Underground.cpp

#include "Underground.hpp"
#include "Background.hpp"
#include "Player.hpp"
#include "DroneManager.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/DebugRenderer.hpp"
#include "../Engine/Collision.hpp"
#include "MapObjectConfig.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace
{
// Other drones use Drone::Init default (100) unless live_drone_states.json overrides them.
// Entry tracer max HP = base * 2; must match ReapplyEntryTracerDroneAfterLiveState().
constexpr float kUndergroundEntryTracerHpBase = 400.0f;
constexpr float kUndergroundEntryTracerMaxHp = kUndergroundEntryTracerHpBase * 2.0f;
constexpr float kUndergroundVendingApproxWidth = 296.0f;
constexpr float kUndergroundTrainRevealRange = 980.0f;
constexpr float kUndergroundTrainTargetLeftExtra = 140.0f;
/// 감속도(월드 유닛/s²) — 낮을수록 더 천천히·길게 진입함
constexpr float kApproachTrainBrakeAccel = 82.0f;
constexpr float kApproachTrainAssumedImageHeight = 1080.0f;
}

// ---------------------------------------------------------------------------
// [ReapplyEntryTracerDroneAfterLiveState]
// - 기능: 라이브 드론 상태 JSON 로드 등으로 인해 오버라이드된 경우, 언더그라운드 구역 입구의 추적 드론의 HP 최대치를 재할당합니다.
// ---------------------------------------------------------------------------
void Underground::ReapplyEntryTracerDroneAfterLiveState()
{
    if (!m_droneManager)
        return;
    std::vector<Drone>& drones = m_droneManager->GetDrones();
    if (drones.empty())
        return;
    Drone& entryTracer = drones[0];
    entryTracer.SetMaxHP(kUndergroundEntryTracerMaxHp);
    entryTracer.SetHP(kUndergroundEntryTracerMaxHp);
    entryTracer.SetTracerHeatLevel(0);
}

// ---------------------------------------------------------------------------
// [Initialize]
// - 기능: 지하철 역사 배경 로드, 진입 연출용 기차 스프라이트 초기화, 드론 7마리 및 정적 물리 궤도 좌표를 할당합니다.
// ---------------------------------------------------------------------------
void Underground::Initialize()
{
    // Initialize background parallax/static image
    m_background = std::make_unique<Background>();
    m_background->Initialize("Asset/SubwayStation.png");

    // Underground 전용 연출 열차: 자판기 근처 접근 시 FirstTrain이 슬라이드 인.
    m_approachTrain = std::make_unique<Background>();
    m_approachTrain->Initialize("Asset/Train/FirstTrain.png");

    m_mapWidth  = DEFAULT_WIDTH;
    m_mapHeight = HEIGHT;
    m_size      = { DEFAULT_WIDTH, HEIGHT };
    m_position  = { MIN_X + DEFAULT_WIDTH * 0.5f, MIN_Y + HEIGHT * 0.5f };
    m_approachTrainCenterY = m_position.y;
    if (m_approachTrain && m_approachTrain->GetWidth() > 0 && m_approachTrain->GetHeight() > 0)
    {
        m_approachTrainWidth  = static_cast<float>(m_approachTrain->GetWidth());
        m_approachTrainHeight = static_cast<float>(m_approachTrain->GetHeight());
    }
    else
    {
        m_approachTrainWidth  = 2472.0f;
        m_approachTrainHeight = HEIGHT;
    }
    m_approachTrainBlend = 0.0f;
    RecalculateApproachTrainAnchors();
    ResetApproachTrainMotion();

    m_droneManager = std::make_unique<DroneManager>();

    // Spawn aerial drones with varying speeds (higher patrol band)
    float droneY = MIN_Y + 550.0f;
    {
        Drone& entryTracer = m_droneManager->SpawnDrone({ MIN_X + 1350.0f, droneY }, "Asset/Drone.png", DroneType::General);
        entryTracer.SetBaseSpeed(55.0f);
        entryTracer.SetMaxHP(kUndergroundEntryTracerMaxHp);
        entryTracer.SetHP(kUndergroundEntryTracerMaxHp);
        entryTracer.SetTracerHeatLevel(0);
    }
    // First patrol drone: shifted left toward robot patrol (~18.2k)
    m_droneManager->SpawnDrone({ 18470.0f, droneY }, "Asset/Drone.png", DroneType::General).SetBaseSpeed(180.0f);
    m_droneManager->SpawnDrone({ 22203.0f, droneY }, "Asset/Drone.png", DroneType::General).SetBaseSpeed(250.0f);
    m_droneManager->SpawnDrone({ 22787.0f, droneY }, "Asset/Drone.png", DroneType::General).SetBaseSpeed(400.0f);
    m_droneManager->SpawnDrone({ 23920.0f, droneY }, "Asset/Drone.png", DroneType::General).SetBaseSpeed(290.0f);

    // Two extra drones skimming much lower (near floor / robot height band)
    const float lowDroneY = MIN_Y + 300.0f;
    // One low drone further ahead (right) than the first high drone
    m_droneManager->SpawnDrone({ 22150.0f, lowDroneY }, "Asset/Drone.png", DroneType::General).SetBaseSpeed(175.0f);
    m_droneManager->SpawnDrone({ 23580.0f, lowDroneY }, "Asset/Drone.png", DroneType::General).SetBaseSpeed(210.0f);

    InitParallaxSkyVAO();
    ApplyConfig(MapObjectConfig::Instance().GetData().underground);
}

// ---------------------------------------------------------------------------
// [RecalculateApproachTrainAnchors]
// - 기능: 진입 연출 열차(Subway Train)의 화면 밖 시작 숨김 X 좌표와 도킹 완료 시의 목표 X 좌표를 맵 가로폭 및 플랫폼 오프셋 기반으로 계산합니다.
// ---------------------------------------------------------------------------
void Underground::RecalculateApproachTrainAnchors()
{
    const float mapRight = MIN_X + m_mapWidth;
    m_approachTrainHiddenCenterX = mapRight + m_approachTrainWidth * 0.5f + 120.0f;
    const float vendingLeft = m_trainBoardingMinWorldX - kUndergroundVendingApproxWidth;
    const float trainTargetLeft = vendingLeft - kUndergroundTrainTargetLeftExtra;
    m_approachTrainTargetCenterX = trainTargetLeft + m_approachTrainWidth * 0.5f;
}

// ---------------------------------------------------------------------------
// [ResetApproachTrainMotion]
// - 기능: 진입 열차 상태를 리셋하여 다시 화면 밖에 대기 상태로 되돌립니다.
// ---------------------------------------------------------------------------
void Underground::ResetApproachTrainMotion()
{
    RecalculateApproachTrainAnchors();
    m_approachTrainCenterX = m_approachTrainHiddenCenterX;
    m_approachTrainTriggered = false;
    m_approachTrainDocked = false;
    m_approachTrainVelX = 0.0f;
    m_approachTrainBlend = 0.0f;
}

// ---------------------------------------------------------------------------
// [ApplyConfig]
// - 기능: JSON 파일의 구성 파라미터(스폰할 로봇 개수, 램프 경사면, 엄폐 상자, 펄스 소스 충전소 목록)를 지하철 역 맵에 로드합니다.
// - 매개변수:
//   - cfg: Underground 맵 오브젝트 설정 레퍼런스
// ---------------------------------------------------------------------------
void Underground::ApplyConfig(const UndergroundObjectConfig& cfg)
{
    m_trainBoardingMinWorldX = MIN_X + cfg.trainBoardingLocalRightX;
    RecalculateApproachTrainAnchors();

    for (auto& source : m_pulseSources) source.Shutdown();
    for (auto& obs : m_obstacles)
    {
        if (obs.sprite) obs.sprite->Shutdown();
    }
    for (auto& lit : m_lights)
    {
        if (lit.sprite) lit.sprite->Shutdown();
    }
    for (auto& robot : m_robots) robot.Shutdown();
    m_pulseSources.clear();
    m_obstacles.clear();
    m_lights.clear();
    m_ramps.clear();
    m_robots.clear();
    m_hidingSpots.clear();

    for (const auto& spawn : cfg.robotSpawns)
    {
        m_robots.emplace_back();
        Robot& r = m_robots.back();
        r.Init(spawn);
        r.ApplyUndergroundDifficultyBoost();
        const float floorY = MIN_Y + 75.0f;
        r.SetGroundLimitY(floorY);
        const Math::Vec2 p = r.GetPosition();
        r.SetPosition({ p.x, floorY + r.GetSize().y * 0.5f });
        r.SetSpawnPosition(r.GetPosition());
    }

    for (const auto& o : cfg.lights)
    {
        if (o.spritePath.empty())
            continue;
        Math::Vec2 sz = o.size;
        if ((sz.x <= 0.0f || sz.y <= 0.0f))
        {
            Background probe;
            probe.Initialize(o.spritePath.c_str());
            const int w = probe.GetWidth();
            const int h = probe.GetHeight();
            probe.Shutdown();
            if (w > 0 && h > 0)
                sz = { static_cast<float>(w), static_cast<float>(h) };
        }
        if (sz.x <= 0.0f || sz.y <= 0.0f)
        {
            if (o.fallbackSize.x > 0.0f && o.fallbackSize.y > 0.0f)
                sz = o.fallbackSize;
            else
                sz = { 1.0f, 1.0f };
        }
        float cx = MIN_X + o.topLeft.x + sz.x * 0.5f;
        float cy = MIN_Y + (HEIGHT - o.topLeft.y) - sz.y * 0.5f;
        LightOverlay layer{};
        layer.pos = { cx, cy };
        layer.size = sz;
        layer.sprite = std::make_unique<Background>();
        layer.sprite->Initialize(o.spritePath.c_str());
        m_lights.push_back(std::move(layer));
    }

    for (const auto& o : cfg.obstacles)
    {
        float cx = MIN_X + o.topLeft.x + o.size.x * 0.5f;
        float cy = MIN_Y + (HEIGHT - o.topLeft.y) - o.size.y * 0.5f;
        Obstacle obs{};
        obs.pos = { cx, cy };
        obs.size = o.size;
        if (!o.spritePath.empty())
        {
            obs.sprite = std::make_unique<Background>();
            obs.sprite->Initialize(o.spritePath.c_str());
        }
        m_obstacles.push_back(std::move(obs));
    }

    for (const auto& h : cfg.hidingSpots)
    {
        float cx = MIN_X + h.topLeft.x + h.size.x * 0.5f;
        float cy = MIN_Y + (HEIGHT - h.topLeft.y) - h.size.y * 0.5f;
        m_hidingSpots.push_back({ { cx, cy }, h.size });
    }

    std::vector<SpriteRectConfig> pulses;
    pulses.reserve(cfg.pulseSources.size());
    for (const auto& p : cfg.pulseSources)
    {
        if (p.spritePath.find("disco") == std::string::npos)
            pulses.push_back(p);
    }
    const size_t pulseCount = pulses.size();
    std::vector<Math::Vec2> effPulseSizes(pulseCount);
    std::vector<Math::Vec2> effPulseTopLeft(pulseCount);

    for (size_t i = 0; i < pulseCount; ++i)
    {
        const auto& p = pulses[i];
        Math::Vec2 sz = p.size;
        if ((sz.x <= 0.0f || sz.y <= 0.0f) && !p.spritePath.empty())
        {
            Background probe;
            probe.Initialize(p.spritePath.c_str());
            const int w = probe.GetWidth();
            const int h = probe.GetHeight();
            probe.Shutdown();
            if (w > 0 && h > 0)
                sz = { static_cast<float>(w), static_cast<float>(h) };
        }
        if (sz.x <= 0.0f || sz.y <= 0.0f)
        {
            if (p.fallbackSize.x > 0.0f && p.fallbackSize.y > 0.0f)
                sz = p.fallbackSize;
            else
                sz = { 1.0f, 1.0f };
        }
        effPulseSizes[i] = sz;
        effPulseTopLeft[i] = p.topLeft;
    }

    std::unordered_map<int, size_t> groupFirstPulseIdx;
    for (size_t i = 0; i < pulseCount; ++i)
    {
        const auto& p = pulses[i];
        const int g = p.sharedPulseGroup;
        if (g == 0)
            continue;
        auto it = groupFirstPulseIdx.find(g);
        if (it == groupFirstPulseIdx.end())
            groupFirstPulseIdx[g] = i;
        else if (p.leaderRightGap >= 0.0f)
        {
            const size_t j = it->second;
            effPulseTopLeft[i].x = effPulseTopLeft[j].x + effPulseSizes[j].x + p.leaderRightGap + p.layoutOffsetX;
            effPulseTopLeft[i].y = p.topLeft.y;
        }
    }

    std::unordered_map<int, size_t> sharedLeaderIdx;
    for (size_t i = 0; i < pulseCount; ++i)
    {
        const auto& p = pulses[i];
        const Math::Vec2& sz = effPulseSizes[i];
        const Math::Vec2& tl = effPulseTopLeft[i];
        float cx = MIN_X + tl.x + sz.x * 0.5f;
        float cy = MIN_Y + (HEIGHT - tl.y) - sz.y * 0.5f;
        m_pulseSources.emplace_back();
        PulseSource& ps = m_pulseSources.back();
        ps.Initialize({ cx, cy }, sz, 100.0f);
        ps.SetHitboxMargin(p.hitboxMargin);
        ps.SetDrawRemainGauge(p.gaugeAnchor);
        if (!p.spritePath.empty())
            ps.InitializeSprite(p.spritePath.c_str());

        if (p.sharedPulseGroup != 0)
        {
            auto it = sharedLeaderIdx.find(p.sharedPulseGroup);
            if (it == sharedLeaderIdx.end())
                sharedLeaderIdx[p.sharedPulseGroup] = m_pulseSources.size() - 1;
            else
                ps.SharePulseStorageWith(m_pulseSources[it->second]);
        }
    }

    for (const auto& r : cfg.ramps)
    {
        float cx = MIN_X + r.topLeft.x + r.size.x * 0.5f;
        float cy = MIN_Y + (HEIGHT - r.topLeft.y) - r.size.y * 0.5f;
        m_ramps.push_back({ {cx, cy}, r.size, true });
    }
}

// ---------------------------------------------------------------------------
// [Update]
// - 기능: 자판기 근처 접근 시 기차 진입 연출(감속 물리), 드론 및 로봇 순찰 AI 구동, 경사면(Ramp) 보정 및 정적 장애물 AABB 충돌 충돌 해결을 총괄합니다.
// - 매개변수:
//   - dt: 프레임 시간 델타
//   - player: 플레이어 레퍼런스
//   - playerHitboxSize: 플레이어 히트박스 크기
// - 가이드라인: 경사면(Ramp)은 플레이어 발끝이 경사면 위에 안착하는 순간 Y 좌표 보정을 위해 선형 보간을 사용합니다.
// ---------------------------------------------------------------------------
void Underground::Update(double dt, Player& player, Math::Vec2 playerHitboxSize)
{
    const float triggerStart = m_trainBoardingMinWorldX - kUndergroundTrainRevealRange;
    if (!m_approachTrainTriggered && player.GetPosition().x >= triggerStart)
    {
        m_approachTrainTriggered = true;
        const float travelSpan =
            std::max(m_approachTrainHiddenCenterX - m_approachTrainTargetCenterX, 1.0f);
        // v₀² = 2·a·d — 정지 지점에서 속도 0이 되도록 초기 속도 설정(관성 진입)
        m_approachTrainVelX = -std::sqrt(2.0f * kApproachTrainBrakeAccel * travelSpan);
    }

    if (m_approachTrainTriggered && !m_approachTrainDocked)
    {
        const float fdt = static_cast<float>(dt);
        // 브레이크: 왼쪽(-) 속도를 매 프레임 줄여 감속
        m_approachTrainVelX += kApproachTrainBrakeAccel * fdt;
        if (m_approachTrainVelX > 0.0f)
            m_approachTrainVelX = 0.0f;

        m_approachTrainCenterX += m_approachTrainVelX * fdt;

        const float distLeft = m_approachTrainCenterX - m_approachTrainTargetCenterX;
        if (distLeft <= 0.0f || (m_approachTrainVelX >= -8.0f && distLeft < 24.0f))
        {
            m_approachTrainCenterX = m_approachTrainTargetCenterX;
            m_approachTrainVelX = 0.0f;
            m_approachTrainDocked = true;
            m_approachTrainBlend = 1.0f;
        }
    }
    else if (!m_approachTrainTriggered)
    {
        m_approachTrainCenterX = m_approachTrainHiddenCenterX;
    }
    m_approachTrainBlend = (m_approachTrainCenterX < m_approachTrainHiddenCenterX - 2.0f) ? 1.0f : 0.0f;

    const bool hide =
        IsPlayerHiding(player.GetHitboxCenter(), playerHitboxSize, player.IsCrouching());
    m_droneManager->Update(dt, player, playerHitboxSize, hide, true, 1.f);

    // Prep obstacle info for robot AI pathfinding/collision
    std::vector<ObstacleInfo> obstacleInfos;
    for (const auto& obs : m_obstacles) {
        obstacleInfos.push_back({ obs.pos, obs.size });
    }

    float mapMinX = MIN_X;
    float mapMaxX = MIN_X + m_mapWidth;

    for (auto& robot : m_robots)
    {
        robot.Update(dt, player, obstacleInfos, mapMinX, mapMaxX);
    }

    // --- Player vs Obstacle Collision Resolution (AABB) ---
    Math::Vec2 currentHitboxCenter = player.GetHitboxCenter();
    Math::Vec2 playerHalfSize = playerHitboxSize / 2.0f;

    struct CollisionTarget { Math::Vec2 pos; Math::Vec2 size; };
    std::vector<CollisionTarget> collisionTargets;
    for (const auto& obs : m_obstacles)
    {
        collisionTargets.push_back({ obs.pos, obs.size });
    }

    if (m_approachTrainDocked && m_approachTrainWidth > 1.0f && m_approachTrainHeight > 1.0f)
    {
        const float left = m_approachTrainCenterX - m_approachTrainWidth * 0.5f;
        const float xScale = m_approachTrainWidth / 2640.0f;
        const float yScale = m_approachTrainHeight / kApproachTrainAssumedImageHeight;
        const float deckX = left + (84.0f + 2472.0f * 0.5f) * xScale;
        const float deckY =
            m_approachTrainCenterY + (kApproachTrainAssumedImageHeight * 0.5f - (804.0f + 45.0f * 0.5f)) * yScale - 20.0f;
        collisionTargets.push_back({ { deckX, deckY }, { 2472.0f * xScale, 85.0f * yScale } });
    }

    auto resolveObsHorizontal = [&](const Math::Vec2& obsCenter, const Math::Vec2& obsMin,
                                    const Math::Vec2& obsMax) {
        Math::Vec2 n = currentHitboxCenter;
        if (currentHitboxCenter.x < obsCenter.x)
            n.x = obsMin.x - playerHalfSize.x;
        else
            n.x = obsMax.x + playerHalfSize.x;
        return n;
    };

    for (const auto& obs : collisionTargets)
    {
        if (Collision::CheckAABB(currentHitboxCenter, playerHitboxSize, obs.pos, obs.size))
        {
            Math::Vec2 obsHalfSize = obs.size / 2.0f;
            Math::Vec2 obsMin = obs.pos - obsHalfSize;
            Math::Vec2 obsMax = obs.pos + obsHalfSize;

            Math::Vec2 playerMin = currentHitboxCenter - playerHalfSize;
            Math::Vec2 playerMax = currentHitboxCenter + playerHalfSize;

            // Calculate penetration depth on both axes
            float overlapX = std::min(playerMax.x, obsMax.x) - std::max(playerMin.x, obsMin.x);
            float overlapY = std::min(playerMax.y, obsMax.y) - std::max(playerMin.y, obsMin.y);

            Math::Vec2 newHitboxCenter = currentHitboxCenter;

            // Resolve along the axis with the smallest overlap
            if (overlapX < overlapY)
            {
                newHitboxCenter = resolveObsHorizontal(obs.pos, obsMin, obsMax);
            }
            else
            {
                if (currentHitboxCenter.y < obs.pos.y)
                {
                    // Hit ceiling
                    newHitboxCenter.y = obsMin.y - playerHalfSize.y;
                    player.ResetVerticalVelocity();
                }
                else
                {
                    // Potential “land on top” — at ledge corners overlapY can beat overlapX and
                    // snap the player forever; require enough foot width on the platform surface.
                    const float platTop    = obsMax.y;
                    const float feetY      = playerMin.y;
                    const float horizOnObs = std::min(playerMax.x, obsMax.x) - std::max(playerMin.x, obsMin.x);
                    constexpr float kMinTopSupportW = 22.0f;
                    constexpr float kFeetAboveTopMax  = 60.0f;
                    constexpr float kFeetBelowTopMax  = 28.0f;

                    const bool nearTopSurface =
                        feetY <= platTop + kFeetBelowTopMax && feetY >= platTop - kFeetAboveTopMax;

                    if (nearTopSurface && horizOnObs >= kMinTopSupportW)
                    {
                        newHitboxCenter.y = platTop + playerHalfSize.y;
                        player.SetOnGround(true);
                    }
                    else if (nearTopSurface && horizOnObs < kMinTopSupportW)
                    {
                        newHitboxCenter = resolveObsHorizontal(obs.pos, obsMin, obsMax);
                    }
                    else
                    {
                        newHitboxCenter.y = obsMax.y + playerHalfSize.y;
                        player.SetOnGround(true);
                    }
                }
            }

            Math::Vec2 shift = newHitboxCenter - currentHitboxCenter;
            player.SetPosition(player.GetPosition() + shift);
            currentHitboxCenter = newHitboxCenter;
        }
    }

    // --- Player vs Ramp Collision Resolution ---
    float playerFootX = currentHitboxCenter.x;
    float playerFootY = currentHitboxCenter.y - playerHalfSize.y;

    for (const auto& ramp : m_ramps)
    {
        float rampHalfW = ramp.size.x / 2.0f;
        float rampHalfH = ramp.size.y / 2.0f;
        float rampLeft = ramp.pos.x - rampHalfW;
        float rampRight = ramp.pos.x + rampHalfW;
        float rampBottom = ramp.pos.y - rampHalfH;
        float rampTop = ramp.pos.y + rampHalfH;

        // Check if player is within ramp horizontal bounds
        if (playerFootX >= rampLeft && playerFootX <= rampRight &&
            playerFootY >= rampBottom && playerFootY <= rampTop + 50.0f)
        {
            // Ignore ramp if jumping upwards
            if (player.GetVelocity().y > 0.0f) continue;

            // Linear interpolation to find the height of the ramp at current X
            float localX = (playerFootX - rampLeft) / ramp.size.x;
            float targetY = rampBottom + (localX * ramp.size.y);

            // Snap player to ramp surface if close enough
            if (playerFootY <= targetY + 10.0f)
            {
                float newCenterY = targetY + playerHalfSize.y;
                Math::Vec2 shift = { 0.0f, newCenterY - currentHitboxCenter.y };
                player.SetPosition(player.GetPosition() + shift);

                player.SetOnGround(true);
                currentHitboxCenter.y = newCenterY;
            }
        }
    }

    if (player.IsOnGround() && player.GetVelocity().y <= 1.f)
    {
        currentHitboxCenter = player.GetHitboxCenter();
        playerHalfSize      = playerHitboxSize * 0.5f;
        const float footY   = currentHitboxCenter.y - playerHalfSize.y;
        const float footX   = currentHitboxCenter.x;
        const float footL   = currentHitboxCenter.x - playerHalfSize.x;
        const float footR   = currentHitboxCenter.x + playerHalfSize.x;
        constexpr float kTol = 20.f;

        bool supported = false;
        const float     gl = player.GetCurrentGroundLevel();
        if (footY >= gl - 28.f && footY <= gl + 32.f)
            supported = true;

        for (const auto& obs : collisionTargets)
        {
            const float halfW = obs.size.x * 0.5f;
            const float halfH = obs.size.y * 0.5f;
            const float top   = obs.pos.y + halfH;
            const float ol    = obs.pos.x - halfW;
            const float orr   = obs.pos.x + halfW;
            if (footR > ol + 4.f && footL < orr - 4.f && std::abs(footY - top) <= kTol)
            {
                supported = true;
                break;
            }
        }

        for (const auto& ramp : m_ramps)
        {
            float        rampHalfW = ramp.size.x / 2.0f;
            float        rampHalfH = ramp.size.y / 2.0f;
            const float  rampLeft  = ramp.pos.x - rampHalfW;
            const float  rampRight = ramp.pos.x + rampHalfW;
            const float  rampBottom = ramp.pos.y - rampHalfH;
            if (footX >= rampLeft && footX <= rampRight && ramp.size.x > 1.f)
            {
                const float localX   = (footX - rampLeft) / ramp.size.x;
                const float targetY  = rampBottom + (localX * ramp.size.y);
                if (std::abs(footY - targetY) <= kTol + 12.f)
                {
                    supported = true;
                    break;
                }
            }
        }

        if (!supported)
            player.SetOnGround(false);
    }
}

// ---------------------------------------------------------------------------
// [InitParallaxSkyVAO]
// - 기능: 석양 하늘 그라데이션 및 팩맨 하늘 그리기를 위한 VAO, VBO 정점 버퍼 데이터를 초기화합니다.
// ---------------------------------------------------------------------------
void Underground::InitParallaxSkyVAO()
{
    float vertices[] = {
        -0.5f,  0.5f,
         0.5f, -0.5f,
        -0.5f, -0.5f,
        -0.5f,  0.5f,
         0.5f,  0.5f,
         0.5f, -0.5f
    };
    GL::GenVertexArrays(1, &m_parallaxSkyVAO);
    GL::GenBuffers(1, &m_parallaxSkyVBO);
    GL::BindVertexArray(m_parallaxSkyVAO);
    GL::BindBuffer(GL_ARRAY_BUFFER, m_parallaxSkyVBO);
    GL::BufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    GL::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    GL::EnableVertexAttribArray(0);
    GL::BindVertexArray(0);
}

// ---------------------------------------------------------------------------
// [DrawFilledQuad]
// - 기능: 단색 셰이더를 사용하여 화면 임의 좌표에 지정한 크기와 색상으로 사각형을 렌더링합니다.
// ---------------------------------------------------------------------------
void Underground::DrawFilledQuad(Shader& colorShader, Math::Vec2 center, Math::Vec2 size, float r, float g,
                                 float b, float a) const
{
    if (!m_parallaxSkyVAO)
        return;

    Math::Matrix model = Math::Matrix::CreateTranslation(center) * Math::Matrix::CreateScale(size);
    colorShader.setMat4("model", model);
    colorShader.setVec3("objectColor", r, g, b);
    colorShader.setFloat("uAlpha", a);

    GL::BindVertexArray(m_parallaxSkyVAO);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);
    GL::BindVertexArray(0);
}

// ---------------------------------------------------------------------------
// [DrawParallaxBackground]
// - 기능: 하늘 석양 그라데이션, 태양, 원경 구름, 빌딩 실루엣, 전신주 등을 패럴랙스 카메라 보정을 반영하여 다중 레이어로 그립니다.
// - 매개변수:
//   - colorShader: 단색 드로잉 셰이더
//   - cameraPos: 현재 카메라 위치
//   - viewHalfW: 화면 반폭 크기
// ---------------------------------------------------------------------------
void Underground::DrawParallaxBackground(Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW) const
{
    if (!m_parallaxSkyVAO)
        return;

    viewHalfW = (viewHalfW > 300.0f) ? viewHalfW : 300.0f;
    const float drawMargin   = 1400.0f;
    const float visibleLeft  = cameraPos.x - viewHalfW - drawMargin;
    const float visibleRight = cameraPos.x + viewHalfW + drawMargin;

    const float skyAnchorY = MIN_Y + HEIGHT * 0.5f;
    const float skyLift    = cameraPos.y - skyAnchorY;
    const auto  relY       = [&](float t) { return MIN_Y + HEIGHT * t + skyLift; };

    const float spanW   = (visibleRight - visibleLeft) + 1200.0f;
    const float centerX = MIN_X + m_mapWidth * 0.5f;
    const float camDx   = cameraPos.x - centerX;
    const float skyPx   = cameraPos.x;
    const float farPx   = centerX + camDx * 0.05f;
    const float midPx   = centerX + camDx * 0.16f;
    const float nearPx  = centerX + camDx * 0.34f;

    DrawFilledQuad(colorShader, { skyPx, relY(0.90f) }, { spanW, HEIGHT * 0.22f }, 0.13f, 0.05f, 0.19f, 1.0f);
    DrawFilledQuad(colorShader, { skyPx, relY(0.75f) }, { spanW, HEIGHT * 0.22f }, 0.22f, 0.08f, 0.20f, 0.95f);
    DrawFilledQuad(colorShader, { skyPx, relY(0.60f) }, { spanW, HEIGHT * 0.20f }, 0.38f, 0.11f, 0.18f, 0.90f);
    DrawFilledQuad(colorShader, { skyPx, relY(0.47f) }, { spanW, HEIGHT * 0.18f }, 0.58f, 0.17f, 0.14f, 0.88f);
    DrawFilledQuad(colorShader, { skyPx, relY(0.36f) }, { spanW, HEIGHT * 0.16f }, 0.80f, 0.28f, 0.11f, 0.85f);
    DrawFilledQuad(colorShader, { skyPx, relY(0.25f) }, { spanW, HEIGHT * 0.18f }, 0.53f, 0.18f, 0.10f, 0.70f);
    DrawFilledQuad(colorShader, { skyPx, relY(0.11f) }, { spanW, HEIGHT * 0.22f }, 0.10f, 0.07f, 0.08f, 1.0f);

    const float sunX = centerX + camDx * 0.9f + 320.0f;
    const float sunY = relY(0.37f);
    DrawFilledQuad(colorShader, { sunX, sunY }, { HEIGHT * 0.34f, HEIGHT * 0.34f }, 1.00f, 0.48f, 0.18f, 0.28f);
    DrawFilledQuad(colorShader, { sunX, sunY }, { HEIGHT * 0.18f, HEIGHT * 0.18f }, 1.00f, 0.62f, 0.24f, 0.58f);
    DrawFilledQuad(colorShader, { sunX, sunY }, { HEIGHT * 0.09f, HEIGHT * 0.09f }, 1.00f, 0.79f, 0.35f, 0.95f);

    const float cloudBase = farPx;
    const float cloudStep = 620.0f;
    const int   cloudMinI = static_cast<int>(std::floor((visibleLeft - cloudBase - 700.0f) / cloudStep));
    const int   cloudMaxI = static_cast<int>(std::ceil((visibleRight - cloudBase + 700.0f) / cloudStep));
    for (int i = cloudMinI; i <= cloudMaxI; ++i)
    {
        const float x  = cloudBase + i * 620.0f;
        const float y1 = relY(0.78f - 0.02f * static_cast<float>((i + 30) % 4));
        const float y2 = relY(0.66f - 0.02f * static_cast<float>((i + 11) % 5));
        const float y3 = relY(0.56f - 0.015f * static_cast<float>((i + 7) % 6));

        DrawFilledQuad(colorShader, { x, y1 }, { 520.0f, 52.0f }, 0.40f, 0.17f, 0.27f, 0.26f);
        DrawFilledQuad(colorShader, { x + 120.0f, y1 - 24.0f }, { 360.0f, 38.0f }, 0.33f, 0.13f, 0.24f, 0.20f);
        DrawFilledQuad(colorShader, { x - 80.0f, y2 }, { 430.0f, 42.0f }, 0.52f, 0.21f, 0.20f, 0.18f);
        DrawFilledQuad(colorShader, { x + 50.0f, y2 - 20.0f }, { 300.0f, 30.0f }, 0.45f, 0.17f, 0.18f, 0.14f);
        DrawFilledQuad(colorShader, { x + 30.0f, y3 }, { 340.0f, 28.0f }, 0.68f, 0.26f, 0.16f, 0.10f);
    }

    const float midStep = 360.0f;
    const int   midMinI = static_cast<int>(std::floor((visibleLeft - midPx - 300.0f) / midStep));
    const int   midMaxI = static_cast<int>(std::ceil((visibleRight - midPx + 300.0f) / midStep));
    for (int i = midMinI; i <= midMaxI; ++i)
    {
        const float x = midPx + i * 360.0f;
        const float h = 110.0f + static_cast<float>((i + 60) % 7) * 26.0f;
        const float w = 130.0f + static_cast<float>((i + 60) % 4) * 22.0f;
        DrawFilledQuad(colorShader, { x, relY(0.13f) + h * 0.5f }, { w, h }, 0.10f, 0.06f, 0.09f, 0.95f);
    }

    const float nearStep = 210.0f;
    const int   nearMinI = static_cast<int>(std::floor((visibleLeft - nearPx - 250.0f) / nearStep));
    const int   nearMaxI = static_cast<int>(std::ceil((visibleRight - nearPx + 250.0f) / nearStep));
    for (int i = nearMinI; i <= nearMaxI; ++i)
    {
        const float x = nearPx + i * 210.0f;
        const float h = 86.0f + static_cast<float>((i + 100) % 5) * 20.0f;
        DrawFilledQuad(colorShader, { x, relY(0.07f) + h * 0.5f }, { 150.0f, h }, 0.07f, 0.05f, 0.06f, 1.0f);
    }

    const float poleStep = 160.0f;
    const int   poleMinI = static_cast<int>(std::floor((visibleLeft - nearPx - 120.0f) / poleStep));
    const int   poleMaxI = static_cast<int>(std::ceil((visibleRight - nearPx + 120.0f) / poleStep));
    for (int i = poleMinI; i <= poleMaxI; ++i)
    {
        const float x = nearPx + i * 160.0f;
        DrawFilledQuad(colorShader, { x, relY(0.22f) },
                       { 10.0f, 170.0f + static_cast<float>((i + 80) % 3) * 36.0f },
                       0.06f, 0.04f, 0.05f, 0.94f);
    }
}

// ---------------------------------------------------------------------------
// [Draw]
// - 기능: 진입 연출 중인 기차, 지하철역 벽면 배경, 라이트 텍스처, 장애물, 로봇 및 펄스 충전소 스프라이트를 렌더링합니다.
// ---------------------------------------------------------------------------
void Underground::Draw(Shader& shader) const
{
    // 열차를 먼저 그리고, 그 위에 배경을 그려 자판기가 열차보다 위 레이어에 오도록 유지.
    if (m_approachTrain && m_approachTrain->GetTextureID() != 0 && m_approachTrainBlend > 0.002f)
    {
        shader.setFloat("alpha", std::clamp(m_approachTrainBlend * 1.15f, 0.0f, 1.0f));
        Math::Matrix trainModel = Math::Matrix::CreateTranslation({ m_approachTrainCenterX, m_approachTrainCenterY })
                                * Math::Matrix::CreateScale({ m_approachTrainWidth, m_approachTrainHeight });
        shader.setMat4("model", trainModel);
        m_approachTrain->Draw(shader, trainModel);
    }

    // Draw background
    shader.setFloat("alpha", 1.0f);
    Math::Matrix model = Math::Matrix::CreateTranslation(m_position) * Math::Matrix::CreateScale(m_size);
    shader.setMat4("model", model);
    m_background->Draw(shader, model);

    shader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
    shader.setBool("flipX", false);
    for (const auto& lit : m_lights)
    {
        if (!lit.sprite) continue;
        Math::Matrix lightModel = Math::Matrix::CreateTranslation(lit.pos) * Math::Matrix::CreateScale(lit.size);
        shader.setMat4("model", lightModel);
        lit.sprite->Draw(shader, lightModel);
    }

    for (const auto& obs : m_obstacles)
    {
        if (!obs.sprite) continue;
        Math::Matrix obsModel = Math::Matrix::CreateTranslation(obs.pos) * Math::Matrix::CreateScale(obs.size);
        shader.setMat4("model", obsModel);
        obs.sprite->Draw(shader, obsModel);
    }

    for (const auto& source : m_pulseSources)
    {
        if (source.HasSprite())
            source.DrawSprite(shader);
    }

    // Draw enemies
    for (const auto& robot : m_robots)
    {
        robot.Draw(shader);
    }
}

// ---------------------------------------------------------------------------
// [IsPlayerOnApproachTrain]
// - 기능: 플레이어가 진입 후 정차 완료한 열차(Subway Train)의 발판 덱(Deck) 위에 완전히 올라타 서있는 상태인지 AABB 검사합니다.
// ---------------------------------------------------------------------------
bool Underground::IsPlayerOnApproachTrain(Math::Vec2 playerHbCenter, Math::Vec2 playerHitboxSize) const
{
    if (!m_approachTrainDocked || m_approachTrainWidth <= 1.0f || m_approachTrainHeight <= 1.0f)
        return false;

    const float left = m_approachTrainCenterX - m_approachTrainWidth * 0.5f;
    const float xScale = m_approachTrainWidth / 2640.0f;
    const float yScale = m_approachTrainHeight / kApproachTrainAssumedImageHeight;
    const float deckX = left + (84.0f + 2472.0f * 0.5f) * xScale;
    const float deckWidth = 2472.0f * xScale;

    // Horizontal check: player must be within the train deck's left/right boundaries.
    const float playerLeft = playerHbCenter.x - playerHitboxSize.x * 0.5f;
    const float playerRight = playerHbCenter.x + playerHitboxSize.x * 0.5f;
    const float trainLeft = deckX - deckWidth * 0.5f;
    const float trainRight = deckX + deckWidth * 0.5f;

    const bool xOverlap = (playerLeft >= m_trainBoardingMinWorldX) && (playerLeft < trainRight);

    // Vertical check: player's feet must be on or above the deck top,
    // but below a high threshold where they might be jumping or standing on containers.
    const float deckY =
        m_approachTrainCenterY + (kApproachTrainAssumedImageHeight * 0.5f - (804.0f + 45.0f * 0.5f)) * yScale - 20.0f;
    const float deckTopY = deckY + 85.0f * yScale * 0.5f;
    const float playerFeetY = playerHbCenter.y - playerHitboxSize.y * 0.5f;

    // We allow a small tolerance below the deck (e.g. -15.f) and a large height above the deck (e.g. +450.f)
    const bool yOverlap = (playerFeetY >= deckTopY - 15.0f) && (playerFeetY <= deckTopY + 450.0f * yScale);

    return xOverlap && yOverlap;
}

// ---------------------------------------------------------------------------
// [DrawDrones]
// - 기능: 공중 순찰 드론 스프라이트를 그립니다.
// ---------------------------------------------------------------------------
void Underground::DrawDrones(Shader& shader) const
{
    m_droneManager->Draw(shader);
}

// ---------------------------------------------------------------------------
// [DrawRadars]
// - 기능: 순찰 드론들의 레이더 범위 선을 그립니다.
// ---------------------------------------------------------------------------
void Underground::DrawRadars(const Shader& colorShader, DebugRenderer& debugRenderer) const
{
    m_droneManager->DrawRadars(colorShader, debugRenderer);
}

// ---------------------------------------------------------------------------
// [DrawGauges]
// - 기능: 드론 상단 경고 및 로봇 HP바, 충돌 예고선(Alert) 레이어를 렌더링합니다.
// ---------------------------------------------------------------------------
void Underground::DrawGauges(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    m_droneManager->DrawGauges(colorShader, debugRenderer);

    for (const auto& robot : m_robots)
    {
        robot.DrawGauge(colorShader, debugRenderer);
        robot.DrawAlert(colorShader, debugRenderer);
    }
}

// ---------------------------------------------------------------------------
// [DrawDebug]
// - 기능: 디버그 모드가 켜진 경우, 장애물 히트박스(적색), 펄스 소스 충전소(주황), 램프 경사면(백색) 히트박스를 표시합니다.
// ---------------------------------------------------------------------------
void Underground::DrawDebug(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    // Draw collision boxes for all environment objects
    for (const auto& obs : m_obstacles)
    {
        debugRenderer.DrawBox(colorShader, obs.pos, obs.size, { 1.0f, 0.0f });
    }

    for (const auto& source : m_pulseSources)
    {
        debugRenderer.DrawBox(colorShader, source.GetPosition(), source.GetHitboxSize(), { 1.0f, 0.5f });
    }

    for (const auto& ramp : m_ramps)
    {
        debugRenderer.DrawBox(colorShader, ramp.pos, ramp.size, { 1.0f, 1.0f });
    }

    if (m_approachTrainDocked && m_approachTrainWidth > 1.0f && m_approachTrainHeight > 1.0f)
    {
        const float left = m_approachTrainCenterX - m_approachTrainWidth * 0.5f;
        const float xScale = m_approachTrainWidth / 2640.0f;
        const float yScale = m_approachTrainHeight / kApproachTrainAssumedImageHeight;
        const float deckX = left + (84.0f + 2472.0f * 0.5f) * xScale;
        const float deckY =
            m_approachTrainCenterY + (kApproachTrainAssumedImageHeight * 0.5f - (804.0f + 45.0f * 0.5f)) * yScale - 20.0f;
        const Math::Vec2 deckSize = { 2472.0f * xScale, 85.0f * yScale };

        // 1) Draw solid collision box in Cyan (0.0f, 0.8f, 1.0f)
        debugRenderer.DrawBox(colorShader, { deckX, deckY }, deckSize, 0.0f, 0.8f, 1.0f);

        // 2) Draw boarding transition trigger box in Yellow (1.0f, 0.9f, 0.0f)
        const float deckWidth = 2472.0f * xScale;
        const float trainRight = deckX + deckWidth * 0.5f;
        const float triggerWidth = trainRight - m_trainBoardingMinWorldX;
        const float triggerCenterX = (m_trainBoardingMinWorldX + trainRight) * 0.5f;

        const float deckTopY = deckY + 85.0f * yScale * 0.5f;
        const float triggerHeight = 450.0f * yScale + 15.0f;
        const float triggerCenterY = deckTopY + (450.0f * yScale - 15.0f) * 0.5f;

        debugRenderer.DrawBox(colorShader, { triggerCenterX, triggerCenterY }, { triggerWidth, triggerHeight }, 1.0f, 0.9f, 0.0f);
    }
}

// ---------------------------------------------------------------------------
// [ApplyPulseToRobots]
// - 기능: 플레이어가 Q 스킬 폭발 펄스를 시전했을 때 범위 내 지하철 로봇들에게 데미지를 입히고 넉백 효과를 줍니다.
// - 매개변수:
//   - pulseWorldCenter: 펄스 구체 중심 좌표
//   - radius: 펄스 반경
// ---------------------------------------------------------------------------
void Underground::ApplyPulseToRobots(Math::Vec2 pulseWorldCenter, float radius)
{
    constexpr float kDamage            = 14.f;
    constexpr float kImpulseCenter     = 900.f;
    constexpr float kImpulseEdge       = 400.f;
    constexpr float kRobotImpulseScale = 0.48f;
    constexpr float kLift              = 135.f;

    const float rSq = radius * radius;
    for (auto& robot : m_robots)
    {
        if (robot.IsDead())
            continue;
        const Math::Vec2 pos = robot.GetPosition();
        Math::Vec2       d   = pos - pulseWorldCenter;
        const float      dSq = d.LengthSq();
        if (dSq > rSq)
            continue;
        const float dist = std::sqrt(dSq);
        float       t    = (dist < 0.1f) ? 1.f : (1.f - dist / (radius + 1.f));
        t                = (std::max)(0.f, t);
        const float      impulseMag = t * (kImpulseCenter - kImpulseEdge) + kImpulseEdge;
        const Math::Vec2 dir        = (dist > 0.1f) ? (d * (1.f / dist)) : Math::Vec2{ 1.f, 0.f };
        const Math::Vec2 impulse    = dir * (impulseMag * kRobotImpulseScale) + Math::Vec2{ 0.f, kLift };
        robot.ApplyPulseImpact(impulse, kDamage);
    }
}

// ---------------------------------------------------------------------------
// [IsPlayerHiding]
// - 기능: 플레이어가 지하철 맵 내의 숨기기 영역( crates 등) 내부에서 웅크리고 있는지 여부를 판정합니다.
// ---------------------------------------------------------------------------
bool Underground::IsPlayerHiding(Math::Vec2 playerHbCenter, Math::Vec2 playerHitboxSize,
                                 bool isPlayerCrouching) const
{
    if (!isPlayerCrouching || m_hidingSpots.empty())
        return false;
    for (const auto& hv : m_hidingSpots)
    {
        if (Collision::CheckAABB(playerHbCenter, playerHitboxSize, hv.center, hv.size))
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// [IsPointOverConfiguredGeometry]
// - 기능: 주어진 마우스 좌표가 맵 내 충돌 장애물, 라이트, 경사면, 엄폐 구역 중 하나 이상과 겹치는지 검사합니다. (커서 프롬프트 판정용)
// ---------------------------------------------------------------------------
bool Underground::IsPointOverConfiguredGeometry(Math::Vec2 worldPos, Math::Vec2 cursorHitboxSize) const
{
    auto test = [&](const Math::Vec2& c, const Math::Vec2& sz) {
        return Collision::CheckPointInAABB(worldPos, c, sz)
            || Collision::CheckAABB(worldPos, cursorHitboxSize, c, sz);
    };
    for (const auto& o : m_obstacles)
    {
        if (test(o.pos, o.size))
            return true;
    }
    for (const auto& lit : m_lights)
    {
        if (test(lit.pos, lit.size))
            return true;
    }
    for (const auto& r : m_ramps)
    {
        if (test(r.pos, r.size))
            return true;
    }
    for (const auto& hv : m_hidingSpots)
    {
        if (test(hv.center, hv.size))
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// [RefillPulseSourcesAfterCheckpointRespawn]
// - 기능: 체크포인트 부활 시 맵 내 펄스 충전소의 충전량을 최대로 복구합니다.
// ---------------------------------------------------------------------------
void Underground::RefillPulseSourcesAfterCheckpointRespawn()
{
    for (auto& s : m_pulseSources)
        s.RefillStock();
}

// ---------------------------------------------------------------------------
// [Shutdown]
// - 기능: 옥상 배경 이미지 해제, 드론 및 로봇 객체의 셧다운 소멸을 수행합니다.
// ---------------------------------------------------------------------------
void Underground::Shutdown()
{
    if (m_parallaxSkyVAO)
    {
        GL::DeleteVertexArrays(1, &m_parallaxSkyVAO);
        m_parallaxSkyVAO = 0;
    }
    if (m_parallaxSkyVBO)
    {
        GL::DeleteBuffers(1, &m_parallaxSkyVBO);
        m_parallaxSkyVBO = 0;
    }

    if (m_background) m_background->Shutdown();
    if (m_approachTrain) m_approachTrain->Shutdown();
    if (m_droneManager) m_droneManager->Shutdown();

    for (auto& obs : m_obstacles)
    {
        if (obs.sprite) obs.sprite->Shutdown();
    }
    for (auto& lit : m_lights)
    {
        if (lit.sprite) lit.sprite->Shutdown();
    }
    for (auto& robot : m_robots) robot.Shutdown();
    for (auto& source : m_pulseSources) source.Shutdown();
}

const std::vector<Drone>& Underground::GetDrones() const { return m_droneManager->GetDrones(); }
std::vector<Drone>& Underground::GetDrones() { return m_droneManager->GetDrones(); }
void Underground::ClearAllDrones() { m_droneManager->ClearAllDrones(); }
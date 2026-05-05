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
}

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

void Underground::Initialize()
{
    // Initialize background parallax/static image
    m_background = std::make_unique<Background>();
    m_background->Initialize("Asset/Underground.png");

    m_size = { WIDTH, HEIGHT };
    m_position = { MIN_X + WIDTH / 2.0f, MIN_Y + HEIGHT / 2.0f };

    m_droneManager = std::make_unique<DroneManager>();

    // Spawn aerial drones with varying speeds (higher patrol band)
    float droneY = MIN_Y + 550.0f;
    {
        Drone& entryTracer = m_droneManager->SpawnDrone({ MIN_X + 1350.0f, droneY }, "Asset/Drone.png", true);
        entryTracer.SetBaseSpeed(55.0f);
        entryTracer.SetMaxHP(kUndergroundEntryTracerMaxHp);
        entryTracer.SetHP(kUndergroundEntryTracerMaxHp);
        entryTracer.SetTracerHeatLevel(0);
    }
    // First patrol drone: shifted left toward robot patrol (~18.2k)
    m_droneManager->SpawnDrone({ 18470.0f, droneY }, "Asset/Drone.png", false).SetBaseSpeed(180.0f);
    m_droneManager->SpawnDrone({ 22203.0f, droneY }, "Asset/Drone.png", false).SetBaseSpeed(250.0f);
    m_droneManager->SpawnDrone({ 22787.0f, droneY }, "Asset/Drone.png", false).SetBaseSpeed(400.0f);
    m_droneManager->SpawnDrone({ 23920.0f, droneY }, "Asset/Drone.png", false).SetBaseSpeed(290.0f);

    // Two extra drones skimming much lower (near floor / robot height band)
    const float lowDroneY = MIN_Y + 300.0f;
    // One low drone further ahead (right) than the first high drone
    m_droneManager->SpawnDrone({ 22150.0f, lowDroneY }, "Asset/Drone.png", false).SetBaseSpeed(175.0f);
    m_droneManager->SpawnDrone({ 23580.0f, lowDroneY }, "Asset/Drone.png", false).SetBaseSpeed(210.0f);

    ApplyConfig(MapObjectConfig::Instance().GetData().underground);
}

void Underground::ApplyConfig(const UndergroundObjectConfig& cfg)
{
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
        m_robots.back().Init(spawn);
        m_robots.back().ApplyUndergroundDifficultyBoost();
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

    const auto& pulses = cfg.pulseSources;
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

void Underground::Update(double dt, Player& player, Math::Vec2 playerHitboxSize)
{
    const bool hide =
        IsPlayerHiding(player.GetHitboxCenter(), playerHitboxSize, player.IsCrouching());
    m_droneManager->Update(dt, player, playerHitboxSize, hide, true, 1.f);

    // Prep obstacle info for robot AI pathfinding/collision
    std::vector<ObstacleInfo> obstacleInfos;
    for (const auto& obs : m_obstacles) {
        obstacleInfos.push_back({ obs.pos, obs.size });
    }

    float mapMinX = MIN_X;
    float mapMaxX = MIN_X + WIDTH;

    for (auto& robot : m_robots)
    {
        robot.Update(dt, player, obstacleInfos, mapMinX, mapMaxX);
    }

    // --- Player vs Obstacle Collision Resolution (AABB) ---
    Math::Vec2 currentHitboxCenter = player.GetHitboxCenter();
    Math::Vec2 playerHalfSize = playerHitboxSize / 2.0f;

    auto resolveObsHorizontal = [&](const Math::Vec2& obsCenter, const Math::Vec2& obsMin,
                                    const Math::Vec2& obsMax) {
        Math::Vec2 n = currentHitboxCenter;
        if (currentHitboxCenter.x < obsCenter.x)
            n.x = obsMin.x - playerHalfSize.x;
        else
            n.x = obsMax.x + playerHalfSize.x;
        return n;
    };

    for (const auto& obs : m_obstacles)
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

        for (const auto& obs : m_obstacles)
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

void Underground::Draw(Shader& shader) const
{
    // Draw background
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

void Underground::DrawDrones(Shader& shader) const
{
    m_droneManager->Draw(shader);
}

void Underground::DrawRadars(const Shader& colorShader, DebugRenderer& debugRenderer) const
{
    m_droneManager->DrawRadars(colorShader, debugRenderer);
}

void Underground::DrawGauges(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    m_droneManager->DrawGauges(colorShader, debugRenderer);

    for (const auto& robot : m_robots)
    {
        robot.DrawGauge(colorShader, debugRenderer);
        robot.DrawAlert(colorShader, debugRenderer);
    }
}

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

}

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

void Underground::Shutdown()
{
    if (m_background) m_background->Shutdown();
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
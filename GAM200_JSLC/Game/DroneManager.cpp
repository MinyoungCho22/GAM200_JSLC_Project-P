//DroneManager.cpp

#include "DroneManager.hpp"
#include "Player.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/DebugRenderer.hpp"
#include <algorithm>
#include <deque>

Drone& DroneManager::SpawnDrone(Math::Vec2 position, const char* texturePath, bool isTracer)
{
    drones.emplace_back();
    drones.back().Init(position, texturePath, isTracer);
    return drones.back();
}

void DroneManager::Update(double dt, const Player& player, Math::Vec2 playerHitboxSize, bool isPlayerUndetectable,
                          bool sirenTracerJamEvade, float sirenTracerSpeedMul)
{
    for (auto& drone : drones)
    {
        drone.Update(dt, player, playerHitboxSize, isPlayerUndetectable, sirenTracerJamEvade, sirenTracerSpeedMul);
    }
}

void DroneManager::Draw(const Shader& shader)
{
    for (const auto& drone : drones)
    {
        drone.Draw(shader);
    }
}

void DroneManager::DrawRadars(const Shader& colorShader, DebugRenderer& debugRenderer) const
{
    for (const auto& drone : drones)
    {
        drone.DrawRadar(colorShader, debugRenderer);
    }
}

void DroneManager::Shutdown()
{
    for (auto& drone : drones)
    {
        drone.Shutdown();
    }
}
void DroneManager::DrawGauges(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    for (const auto& drone : drones)
    {
        drone.DrawGauge(colorShader, debugRenderer);
    }
}

void DroneManager::ClearAllDrones()
{
    for (auto& drone : drones)
    {
        drone.Shutdown();
    }
    drones.clear();
}

void DroneManager::ResetAllDrones()
{
    for (auto& drone : drones)
        drone.Reset();
}

std::vector<std::pair<Math::Vec2, Math::Vec2>> DroneManager::ApplyDetonation(
    Math::Vec2 origin, float radius, float stunDuration,
    float chainRange, int maxChains)
{
    std::vector<std::pair<Math::Vec2, Math::Vec2>> arcs;

    constexpr float PRIMARY_DAMAGE   = 25.f;
    constexpr float CHAIN_DAMAGE     = 15.f;
    // Shockwave propagation speed: closer drones react first (domino)
    // delay = dist / WAVE_SPEED  →  at 300px: 0.25s, at 600px: 0.50s
    constexpr float WAVE_SPEED       = 1200.f;
    // Ice-slide impulse: stronger at center
    constexpr float IMPULSE_CENTER   = 900.f;  // px/s at origin
    constexpr float IMPULSE_EDGE     = 400.f;  // px/s at radius edge

    // Phase 1: collect drones in radius, sort closest-first (domino order)
    const float radiusSq = radius * radius;
    struct Hit { size_t idx; float dist; };
    std::vector<Hit> hits;

    for (size_t i = 0; i < drones.size(); ++i)
    {
        if (drones[i].IsDead()) continue;
        Math::Vec2 toTarget = drones[i].GetPosition() - origin;
        float distSq = toTarget.LengthSq();
        if (distSq > radiusSq) continue;
        hits.push_back({ i, std::sqrt(distSq) });
    }

    // Sort: nearest drone first → wave reaches them first
    std::sort(hits.begin(), hits.end(),
        [](const Hit& a, const Hit& b) { return a.dist < b.dist; });

    std::vector<size_t> frontier;
    for (const auto& h : hits)
    {
        float dist  = h.dist;
        size_t i    = h.idx;
        Math::Vec2 toTarget = drones[i].GetPosition() - origin;

        // Delay proportional to distance (shockwave travel time)
        float delay = dist / WAVE_SPEED;

        // Impulse magnitude: linear falloff center→edge
        float t       = (dist < 0.1f) ? 1.f : (1.f - dist / (radius + 1.f));
        t             = (std::max)(0.f, t);
        float impulse = t * (IMPULSE_CENTER - IMPULSE_EDGE) + IMPULSE_EDGE;

        Math::Vec2 dir = (dist > 0.1f) ? toTarget / dist : Math::Vec2{ 1.f, 0.f };

        // Show source-to-target shock links as well, so train-car Q casts visibly "reach" drones.
        arcs.push_back({ origin, drones[i].GetPosition() });

        drones[i].ApplyStun(stunDuration);
        drones[i].ApplyKnockback(dir * impulse, delay);
        // Damage appears after the slide finishes (delay + full slide window)
        drones[i].QueueDamage(PRIMARY_DAMAGE, delay + 0.75f);

        frontier.push_back(i);
    }

    // Phase 2: chain — proper BFS (queue, front-pop) from stunned drones to nearest non-stunned
    // Using deque so we process in wave order rather than depth-first
    const float chainRangeSq = chainRange * chainRange;
    int chainsLeft = maxChains;
    int chainHop   = 0;

    // Convert frontier vector to a queue (front = oldest = closest to origin)
    std::deque<size_t> bfsQueue(frontier.begin(), frontier.end());

    while (!bfsQueue.empty() && chainsLeft > 0)
    {
        size_t srcIdx = bfsQueue.front();
        bfsQueue.pop_front();
        Math::Vec2 srcPos = drones[srcIdx].GetPosition();

        // Find ALL unstunned, living drones within chainRange of this source
        // (not just nearest one) — guarantees adjacent drones always get chained
        for (size_t i = 0; i < drones.size() && chainsLeft > 0; ++i)
        {
            if (drones[i].IsDead() || drones[i].IsStunned()) continue;
            Math::Vec2 tgtPos = drones[i].GetPosition();
            float dSq = (tgtPos - srcPos).LengthSq();
            if (dSq > chainRangeSq) continue;

            Math::Vec2 chainDir = tgtPos - srcPos;
            float chainLen      = chainDir.Length();

            arcs.push_back({ srcPos, tgtPos });

            float chainDelay = (chainHop + 1) * 0.12f;
            Math::Vec2 dir   = (chainLen > 0.1f) ? chainDir / chainLen : Math::Vec2{ 1.f, 0.f };

            drones[i].ApplyStun(stunDuration);
            drones[i].ApplyKnockback(dir * 500.f, chainDelay);
            drones[i].QueueDamage(CHAIN_DAMAGE, chainDelay + 0.75f);

            bfsQueue.push_back(i);
            ++chainHop;
            --chainsLeft;
        }
    }

    return arcs;
}

const std::vector<Drone>& DroneManager::GetDrones() const
{
    return drones;
}

std::vector<Drone>& DroneManager::GetDrones()
{
    return drones;
}
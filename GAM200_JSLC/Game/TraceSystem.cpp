// TraceSystem.cpp

#include "TraceSystem.hpp"
#include "DroneManager.hpp"
#include "Train.hpp"
#include "../Engine/Logger.hpp"
#include <algorithm>
#include <cmath>

void TraceSystem::Initialize()
{
    Logger::Instance().Log(Logger::Severity::Info, "Trace System Initialized.");
}

void TraceSystem::OnDroneKilled(DroneManager& droneManager, Math::Vec2 spawnOrigin)
{
    m_killCount++;
    Logger::Instance().Log(Logger::Severity::Verbose, "Drone kill count: %d", m_killCount);

    // Progression Logic: Level 1 starts after the 1st kill
    if (m_killCount == 1 && m_warningLevel == 0)
    {
        m_warningLevel = 1;
        SpawnTracerWave(droneManager, m_warningLevel, spawnOrigin);
    }
    // Progression Logic: Level 2 starts after the 5th kill
    else if (m_killCount == 5 && m_warningLevel == 1)
    {
        m_warningLevel = 2;
        SpawnTracerWave(droneManager, m_warningLevel, spawnOrigin);
        Logger::Instance().Log(Logger::Severity::Verbose, "Warning Level 2 Reached!");
    }
}

namespace
{
bool IsTracerSpawnOnTrainWorld(Math::Vec2 worldPos)
{
    return worldPos.x >= Train::MIN_X - 400.f && worldPos.x <= Train::MIN_X + 52000.f
        && worldPos.y >= Train::MIN_Y - 120.f && worldPos.y <= Train::MIN_Y + Train::HEIGHT + 200.f;
}
} // namespace

void TraceSystem::SpawnTracerWave(DroneManager& droneManager, int warningLevel, Math::Vec2 origin)
{
    Logger::Instance().Log(Logger::Severity::Verbose, "Spawning tracer wave! (Level %d) at (%.1f, %.1f)", warningLevel,
                            origin.x, origin.y);

    constexpr float PI = 3.14159265359f;

    auto spawnOne = [&](float ang, float radius, float speed, float angJitter) {
        const float a  = ang + angJitter;
        const float rx = std::cos(a) * radius;
        const float ry = std::sin(a) * radius * 0.62f;
        Drone&      d  = droneManager.SpawnDrone({ origin.x + rx, origin.y + ry }, "Asset/RedDrone.png", true);
        d.SetBaseSpeed(speed);
        d.SetTracerHeatLevel(warningLevel);
        if (IsTracerSpawnOnTrainWorld({ origin.x + rx, origin.y + ry }))
            Train::ApplyCombatDroneVisualScale(d);
    };

    if (warningLevel == 1)
    {
        const float r0 = 560.f;
        spawnOne(-0.15f * PI, r0 + 40.f, 58.f, 0.04f);
        spawnOne(0.10f * PI, r0 + 10.f, 98.f, -0.05f);
        spawnOne(0.38f * PI, r0 + 70.f, 138.f, 0.03f);
    }
    else if (warningLevel == 2)
    {
        const float r0 = 520.f;
        spawnOne(-0.22f * PI, r0 + 90.f, 52.f, 0.02f);
        spawnOne(-0.02f * PI, r0 + 20.f, 78.f, -0.04f);
        spawnOne(0.18f * PI, r0 + 55.f, 108.f, 0.05f);
        spawnOne(0.42f * PI, r0 + 35.f, 68.f, -0.03f);
        spawnOne(0.62f * PI, r0 + 80.f, 152.f, 0.04f);
    }
}

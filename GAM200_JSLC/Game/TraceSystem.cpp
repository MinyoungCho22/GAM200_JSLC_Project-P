// TraceSystem.cpp

#include "TraceSystem.hpp"
#include "DroneManager.hpp"
#include "Train.hpp"
#include "../Engine/Logger.hpp"
#include <cmath>

void TraceSystem::Initialize()
{
    Reset();
    Logger::Instance().Log(Logger::Severity::Info, "Trace System Initialized.");
}

void TraceSystem::Reset()
{
    m_killCount = 0;
}

void TraceSystem::OnDroneKilled(DroneManager& droneManager, Math::Vec2 spawnOrigin, TraceStage stage)
{
    m_killCount++;
    Logger::Instance().Log(Logger::Severity::Verbose, "Drone kill count: %d (trace stage %d)", m_killCount,
                            static_cast<int>(stage));
    SpawnTracerWave(droneManager, stage, spawnOrigin);
}

namespace
{
bool IsTracerSpawnOnTrainWorld(Math::Vec2 worldPos)
{
    return worldPos.x >= Train::MIN_X - 400.f && worldPos.x <= Train::MIN_X + 52000.f
        && worldPos.y >= Train::MIN_Y - 120.f && worldPos.y <= Train::MIN_Y + Train::HEIGHT + 200.f;
}
} // namespace

void TraceSystem::SpawnTracerWave(DroneManager& droneManager, TraceStage stage, Math::Vec2 origin)
{
    Logger::Instance().Log(Logger::Severity::Verbose, "Spawning tracer wave (stage %d) at (%.1f, %.1f)",
                            static_cast<int>(stage), origin.x, origin.y);

    constexpr float PI = 3.14159265359f;
    const int       heatLevel = static_cast<int>(stage);

    auto spawnOne = [&](float ang, float radius, float speed, float angJitter) {
        const float a  = ang + angJitter;
        const float rx = std::cos(a) * radius;
        const float ry = std::sin(a) * radius * 0.62f;
        Drone&      d  = droneManager.SpawnDrone({ origin.x + rx, origin.y + ry }, "Asset/RedDrone.png", DroneType::Tracer);
        d.SetBaseSpeed(speed);
        d.SetTracerHeatLevel(heatLevel);
        if (IsTracerSpawnOnTrainWorld({ origin.x + rx, origin.y + ry }))
            Train::ApplyCombatDroneVisualScale(d);
    };

    // Always spawn 3 tracer drones when triggered
    const float r0 = 520.f;
    spawnOne(-0.22f * PI, r0 + 90.f, 52.f, 0.02f);
    spawnOne(-0.02f * PI, r0 + 20.f, 78.f, -0.04f);
    spawnOne(0.18f * PI, r0 + 55.f, 108.f, 0.05f);
}

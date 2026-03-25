// TraceSystem.cpp

#include "TraceSystem.hpp"
#include "DroneManager.hpp"
#include "../Engine/Logger.hpp"
#include <algorithm>

void TraceSystem::Initialize()
{
    Logger::Instance().Log(Logger::Severity::Info, "Trace System Initialized.");
}

void TraceSystem::OnDroneKilled(DroneManager& droneManager, Math::Vec2 spawnOrigin)
{
    m_killCount++;
    Logger::Instance().Log(Logger::Severity::Event, "Drone kill count: %d", m_killCount);

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
        Logger::Instance().Log(Logger::Severity::Event, "Warning Level 2 Reached!");
    }
}

void TraceSystem::SpawnTracerWave(DroneManager& droneManager, int warningLevel, Math::Vec2 origin)
{
    Logger::Instance().Log(Logger::Severity::Event, "Spawning tracer wave! (Level %d) at (%.1f, %.1f)", warningLevel, origin.x, origin.y);

    // Reinforcements appear at an offset from the original kill site
    float offsetX = 600.0f;
    float offsetY = 300.0f;

    if (warningLevel == 1)
    {
        // Spawn a small squad of 3 drones with varying speeds
        droneManager.SpawnDrone({ origin.x + offsetX, origin.y + offsetY }, "Asset/Drone.png", true).SetBaseSpeed(100.0f);
        droneManager.SpawnDrone({ origin.x + offsetX + 50.0f, origin.y + offsetY + 100.0f }, "Asset/Drone.png", true).SetBaseSpeed(150.0f);
        droneManager.SpawnDrone({ origin.x + offsetX - 50.0f, origin.y + offsetY + 200.0f }, "Asset/Drone.png", true).SetBaseSpeed(200.0f);
    }
    else if (warningLevel == 2)
    {
        // Spawn a larger squad of 5 drones for higher difficulty
        droneManager.SpawnDrone({ origin.x + offsetX, origin.y + offsetY }, "Asset/Drone.png", true).SetBaseSpeed(100.0f);
        droneManager.SpawnDrone({ origin.x + offsetX + 50.0f, origin.y + offsetY + 100.0f }, "Asset/Drone.png", true).SetBaseSpeed(150.0f);
        droneManager.SpawnDrone({ origin.x + offsetX - 50.0f, origin.y + offsetY + 200.0f }, "Asset/Drone.png", true).SetBaseSpeed(200.0f);
        droneManager.SpawnDrone({ origin.x + offsetX + 100.0f, origin.y + offsetY + 50.0f }, "Asset/Drone.png", true).SetBaseSpeed(120.0f);
        droneManager.SpawnDrone({ origin.x + offsetX - 100.0f, origin.y + offsetY + 250.0f }, "Asset/Drone.png", true).SetBaseSpeed(180.0f);
    }
}
#include "TraceSystem.hpp"
#include "DroneManager.hpp"
#include "../Engine/Logger.hpp"
#include <algorithm>

void TraceSystem::Initialize()
{
    Logger::Instance().Log(Logger::Severity::Info, "Trace System Initialized.");
}

void TraceSystem::OnDroneKilled(DroneManager& droneManager)
{
    m_killCount++;
    Logger::Instance().Log(Logger::Severity::Event, "Drone kill count: %d", m_killCount);

    if (m_killCount == 1 && m_warningLevel == 0)
    {
        m_warningLevel = 1;
        SpawnTracerWave(droneManager);
    }
    else if (m_killCount == 5 && m_warningLevel == 1)
    {
        m_warningLevel = 2;
        SpawnTracerWave(droneManager);
        Logger::Instance().Log(Logger::Severity::Event, "Warning Level 2 Reached!");
    }
}

void TraceSystem::SpawnTracerWave(DroneManager& droneManager)
{
    Logger::Instance().Log(Logger::Severity::Event, "Spawning tracer wave!");

    droneManager.SpawnDrone({ 1830.0f, 300.0f }, "Asset/drone.png", true).SetBaseSpeed(100.0f);
    droneManager.SpawnDrone({ 1820.0f, 500.0f }, "Asset/drone.png", true).SetBaseSpeed(150.0f);
    droneManager.SpawnDrone({ 1810.0f, 600.0f }, "Asset/drone.png", true).SetBaseSpeed(200.0f);
}
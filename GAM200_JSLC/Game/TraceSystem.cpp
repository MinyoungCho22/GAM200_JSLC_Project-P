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
        SpawnTracerWave(droneManager, m_warningLevel);
    }
    else if (m_killCount == 5 && m_warningLevel == 1)
    {
        m_warningLevel = 2;
        SpawnTracerWave(droneManager, m_warningLevel);
        Logger::Instance().Log(Logger::Severity::Event, "Warning Level 2 Reached!");
    }
}

void TraceSystem::SpawnTracerWave(DroneManager& droneManager, int warningLevel)
{
    Logger::Instance().Log(Logger::Severity::Event, "Spawning tracer wave! (Level %d)", warningLevel);

    if (warningLevel == 1)
    {
        droneManager.SpawnDrone({ 1830.0f, 300.0f }, "Asset/drone.png", true).SetBaseSpeed(100.0f);
        droneManager.SpawnDrone({ 1820.0f, 500.0f }, "Asset/drone.png", true).SetBaseSpeed(150.0f);
        droneManager.SpawnDrone({ 1810.0f, 600.0f }, "Asset/drone.png", true).SetBaseSpeed(200.0f);
    }
    else if (warningLevel == 2)
    {
        droneManager.SpawnDrone({ 1830.0f, 300.0f }, "Asset/drone.png", true).SetBaseSpeed(100.0f);
        droneManager.SpawnDrone({ 1820.0f, 500.0f }, "Asset/drone.png", true).SetBaseSpeed(150.0f);
        droneManager.SpawnDrone({ 1810.0f, 600.0f }, "Asset/drone.png", true).SetBaseSpeed(200.0f);
        droneManager.SpawnDrone({ 1800.0f, 400.0f }, "Asset/drone.png", true).SetBaseSpeed(120.0f); // 추가
        droneManager.SpawnDrone({ 1790.0f, 700.0f }, "Asset/drone.png", true).SetBaseSpeed(180.0f); // 추가
    }
}
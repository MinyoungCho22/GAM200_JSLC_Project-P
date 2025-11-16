//TraceSystem.hpp

#pragma once
#include <memory>

class DroneManager;

class TraceSystem
{
public:

    void Initialize();
    void OnDroneKilled(DroneManager& droneManager);

private:

    void SpawnTracerWave(DroneManager& droneManager, int warningLevel);

    int m_killCount = 0;
    int m_warningLevel = 0;
};
#pragma once
#include <memory>

class DroneManager;

class TraceSystem
{
public:
    void Initialize();
    void OnDroneKilled(DroneManager& droneManager);

private:
    // warningLevel 파라미터 추가
    void SpawnTracerWave(DroneManager& droneManager, int warningLevel);

    int m_killCount = 0;
    int m_warningLevel = 0;
};
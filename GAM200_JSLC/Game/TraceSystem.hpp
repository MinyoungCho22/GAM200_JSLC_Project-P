#pragma once
#include "../Engine/Vec2.hpp"

class DroneManager;

class TraceSystem
{
public:
    void Initialize();

    void OnDroneKilled(DroneManager& droneManager, Math::Vec2 spawnOrigin);

private:
    void SpawnTracerWave(DroneManager& droneManager, int warningLevel, Math::Vec2 origin);

    int m_killCount = 0;
    int m_warningLevel = 0;
};
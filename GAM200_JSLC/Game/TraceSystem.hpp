// TraceSystem.hpp

#pragma once
#include "../Engine/Vec2.hpp"

class DroneManager;

enum class TraceStage
{
    Stage1 = 1, // Room, Hallway, Rooftop
    Stage2 = 2  // Underground, Train, etc.
};

/**
 * @class TraceSystem
 * @brief Spawns RedDrone tracers when the player destroys map drones.
 * Stage1 maps spawn 2 tracers per kill; Stage2 maps spawn 3.
 */
class TraceSystem
{
public:
    void Initialize();
    void Reset();

    /**
     * @brief Spawns a tracer reinforcement wave for the current map stage.
     */
    void OnDroneKilled(DroneManager& droneManager, Math::Vec2 spawnOrigin, TraceStage stage);

    int GetKillCount() const { return m_killCount; }

private:
    void SpawnTracerWave(DroneManager& droneManager, TraceStage stage, Math::Vec2 origin);

    int m_killCount = 0;
};

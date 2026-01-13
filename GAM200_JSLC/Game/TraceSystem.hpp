// TraceSystem.hpp

#pragma once
#include "../Engine/Vec2.hpp"

class DroneManager;

/**
 * @class TraceSystem
 * @brief Manages the "Warning Level" (Heat system) and spawns reinforcements 
 * based on the number of drones the player has destroyed.
 */
class TraceSystem
{
public:
    void Initialize();

    /**
     * @brief Triggered when a drone is destroyed. Increases kill count and updates warning levels.
     * @param droneManager Reference to the manager to spawn new drones.
     * @param spawnOrigin The location from which the reinforcement wave calculation starts.
     */
    void OnDroneKilled(DroneManager& droneManager, Math::Vec2 spawnOrigin);
    
    int GetWarningLevel() const { return m_warningLevel; }

private:
    /**
     * @brief Spawns a group of "Tracer" drones at the specified origin based on current heat.
     */
    void SpawnTracerWave(DroneManager& droneManager, int warningLevel, Math::Vec2 origin);

    int m_killCount = 0;    // Total drones destroyed by the player
    int m_warningLevel = 0; // Current escalation state (0: None, 1: Low, 2: High)
};
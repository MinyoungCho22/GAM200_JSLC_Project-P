//DroneManager.hpp

#pragma once
#include <vector>
#include <utility>
#include "Drone.hpp"

class Shader;
class Player;
class DebugRenderer;

class DroneManager
{
public:
    Drone& SpawnDrone(Math::Vec2 position, const char* texturePath, bool isTracer = false);
    void Update(double dt, const Player& player, Math::Vec2 playerHitboxSize, bool isPlayerHiding);
    void Draw(const Shader& shader);
    void DrawRadars(const Shader& colorShader, DebugRenderer& debugRenderer) const;
    void DrawGauges(Shader& colorShader, DebugRenderer& debugRenderer) const;
    void Shutdown();
    void ClearAllDrones();
    void ResetAllDrones();

    // Returns list of chain arc pairs (from→to world positions) for VFX.
    // Drones in initial radius are stunned first, then the pulse chains to
    // nearby drones up to maxChains hops within chainRange.
    std::vector<std::pair<Math::Vec2, Math::Vec2>> ApplyDetonation(
        Math::Vec2 origin, float radius, float stunDuration,
        float chainRange = 600.f, int maxChains = 10);

    const std::vector<Drone>& GetDrones() const;
    std::vector<Drone>& GetDrones();

private:
    std::vector<Drone> drones;
};
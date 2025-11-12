#pragma once
#include <vector>
#include "Drone.hpp"

class Shader;
class Player;
class DebugRenderer;

class DroneManager
{
public:
    void SpawnDrone(Math::Vec2 position, const char* texturePath);
    void Update(double dt, const Player& player, Math::Vec2 playerHitboxSize, bool isPlayerHiding);
    void Draw(const Shader& shader);
    void DrawRadars(const Shader& colorShader, DebugRenderer& debugRenderer) const;
    void Shutdown();

    const std::vector<Drone>& GetDrones() const;
    std::vector<Drone>& GetDrones();

private:
    std::vector<Drone> drones;
};
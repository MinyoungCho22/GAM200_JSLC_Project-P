// DroneManager.cpp
#include "DroneManager.hpp"
#include "Player.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/DebugRenderer.hpp"

void DroneManager::SpawnDrone(Math::Vec2 position, const char* texturePath)
{
    drones.emplace_back();
    drones.back().Init(position, texturePath);
}

void DroneManager::Update(double dt, const Player& player, Math::Vec2 playerHitboxSize)
{
    for (auto& drone : drones)
    {
        drone.Update(dt, player, playerHitboxSize);
    }
}

// DroneManager.cpp
void DroneManager::Draw(const Shader& shader)
{
    for (const auto& drone : drones)
    {
        drone.Draw(shader);
    }
}

void DroneManager::DrawRadars(const Shader& colorShader, DebugRenderer& debugRenderer) const
{
    for (const auto& drone : drones)
    {
        drone.DrawRadar(colorShader, debugRenderer);
    }
}

void DroneManager::Shutdown()
{
    for (auto& drone : drones)
    {
        drone.Shutdown();
    }
}

const std::vector<Drone>& DroneManager::GetDrones() const
{
    return drones;
}

std::vector<Drone>& DroneManager::GetDrones()
{
    return drones;
}
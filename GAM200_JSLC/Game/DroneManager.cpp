//DroneManager.cpp

#include "DroneManager.hpp"
#include "Player.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/DebugRenderer.hpp"

Drone& DroneManager::SpawnDrone(Math::Vec2 position, const char* texturePath, bool isTracer)
{
    drones.emplace_back();
    drones.back().Init(position, texturePath, isTracer);
    return drones.back();
}

void DroneManager::Update(double dt, const Player& player, Math::Vec2 playerHitboxSize, bool isPlayerHiding)
{
    for (auto& drone : drones)
    {
        drone.Update(dt, player, playerHitboxSize, isPlayerHiding);
    }
}

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

void DroneManager::ClearAllDrones()
{
    for (auto& drone : drones)
    {
        drone.Shutdown();
    }
    drones.clear();
}

const std::vector<Drone>& DroneManager::GetDrones() const
{
    return drones;
}

std::vector<Drone>& DroneManager::GetDrones()
{
    return drones;
}
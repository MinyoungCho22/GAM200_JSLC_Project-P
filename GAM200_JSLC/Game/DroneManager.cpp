#include "DroneManager.hpp"
#include "../OpenGL/Shader.hpp"

void DroneManager::SpawnDrone(Math::Vec2 position, const char* texturePath)
{
    drones.emplace_back();
    drones.back().Init(position, texturePath);
}

void DroneManager::Update(double dt)
{
    for (auto& drone : drones)
    {
        drone.Update(dt);
    }
}

void DroneManager::Draw(const Shader& shader)
{
    for (const auto& drone : drones)
    {
        drone.Draw(shader);
    }
}

void DroneManager::Shutdown()
{
    for (auto& drone : drones)
    {
        drone.Shutdown();
    }
}
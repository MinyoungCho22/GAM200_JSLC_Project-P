#pragma once
#include <vector>
#include "Drone.hpp"

class Shader;

class DroneManager
{
public:
    void SpawnDrone(Math::Vec2 position, const char* texturePath);
    void Update(double dt);
    void Draw(const Shader& shader);
    void Shutdown();

private:
    std::vector<Drone> drones;
};
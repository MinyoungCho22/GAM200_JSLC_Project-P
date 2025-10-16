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

    const std::vector<Drone>& GetDrones() const;
    std::vector<Drone>& GetDrones();

private:
    // [�߰�] Drone ��ü���� ������ ���� ��� ����
    std::vector<Drone> drones;
};
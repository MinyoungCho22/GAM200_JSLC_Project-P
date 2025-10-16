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

    // [�߰�] ��� ��Ͽ� ���� const ������ ��ȯ�ϴ� �Լ�
    const std::vector<Drone>& GetDrones() const;

private:
    std::vector<Drone> drones;
};
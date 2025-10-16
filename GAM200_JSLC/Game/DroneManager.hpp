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

    // [추가] 드론 목록에 대한 const 참조를 반환하는 함수
    const std::vector<Drone>& GetDrones() const;

private:
    std::vector<Drone> drones;
};
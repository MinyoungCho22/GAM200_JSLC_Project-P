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
    // [추가] Drone 객체들을 저장할 벡터 멤버 변수
    std::vector<Drone> drones;
};
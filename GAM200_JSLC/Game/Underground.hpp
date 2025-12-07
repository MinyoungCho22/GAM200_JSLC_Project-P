#pragma once
#include "../Engine/Vec2.hpp"
#include <memory>
#include <vector>

class Background;
class Shader;
class Player;
class DroneManager;
class Drone;
class DebugRenderer;

class Underground
{
public:
    static constexpr float WIDTH = 7920.0f;
    static constexpr float HEIGHT = 1080.0f;
    static constexpr float MIN_X = 16260.0f; // Rooftop(8400) + Hallway(1920) 등을 고려해 우측에 배치
    static constexpr float MIN_Y = -2000.0f;

    void Initialize();
    void Update(double dt, Player& player, Math::Vec2 playerHitboxSize);
    void Draw(Shader& shader) const;
    void DrawRadars(const Shader& colorShader, DebugRenderer& debugRenderer) const;
    void DrawGauges(Shader& colorShader, DebugRenderer& debugRenderer) const;
    void DrawDebug(Shader& colorShader, DebugRenderer& debugRenderer) const;
    void Shutdown();

    const std::vector<Drone>& GetDrones() const;
    std::vector<Drone>& GetDrones();
    void ClearAllDrones();

private:
    std::unique_ptr<Background> m_background;
    Math::Vec2 m_position;
    Math::Vec2 m_size;
    std::unique_ptr<DroneManager> m_droneManager;
};
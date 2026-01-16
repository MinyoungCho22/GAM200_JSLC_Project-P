//Underground.hpp

#pragma once
#include "../Engine/Vec2.hpp"
#include "../Game/PulseSource.hpp"
#include "Robot.hpp" 
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
    static constexpr float MIN_X = 16260.0f;
    static constexpr float MIN_Y = -2000.0f;

    struct Obstacle
    {
        Math::Vec2 pos;
        Math::Vec2 size;
    };

    struct Ramp
    {
        Math::Vec2 pos;
        Math::Vec2 size;
        bool isLeftLow;
    };

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

    std::vector<PulseSource>& GetPulseSources() { return m_pulseSources; }

    std::vector<Robot>& GetRobots() { return m_robots; }

private:
    std::unique_ptr<Background> m_background;
    Math::Vec2 m_position;
    Math::Vec2 m_size;
    std::unique_ptr<DroneManager> m_droneManager;

    std::vector<Obstacle> m_obstacles;
    std::vector<Ramp> m_ramps;
    std::vector<PulseSource> m_pulseSources;
    std::vector<Robot> m_robots;
};
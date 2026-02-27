// Subway.hpp

#pragma once
#include "Background.hpp"
#include "PulseSource.hpp"
#include "DroneManager.hpp"
#include "Robot.hpp"
#include "../Engine/Vec2.hpp"
#include "../Engine/Input.hpp"
#include <memory>
#include <vector>

class Shader;
class Player;
class DebugRenderer;

/**
 * @class Subway
 * @brief Subway map - Final stage transitioning from Underground
 */
class Subway
{
public:
    // Map boundaries and coordinate constants
    static constexpr float WIDTH = 7920.0f;
    static constexpr float HEIGHT = 1080.0f;
    static constexpr float MIN_X = 24180.0f;  // Position after Underground
    static constexpr float MIN_Y = -3500.0f;  // Below Underground

    struct Obstacle
    {
        Math::Vec2 pos;
        Math::Vec2 size;
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
    DroneManager* GetDroneManager() { return m_droneManager.get(); }
    void ClearAllDrones();

    std::vector<PulseSource>& GetPulseSources() { return m_pulseSources; }
    std::vector<Robot>& GetRobots() { return m_robots; }

private:
    std::unique_ptr<Background> m_background;  // Subway.png
    std::unique_ptr<Background> m_subwayTrain; // Subway train image (optional)
    
    Math::Vec2 m_position;
    Math::Vec2 m_size;
    
    std::unique_ptr<DroneManager> m_droneManager;
    
    std::vector<Obstacle> m_obstacles;
    std::vector<PulseSource> m_pulseSources;
    std::vector<Robot> m_robots;
};

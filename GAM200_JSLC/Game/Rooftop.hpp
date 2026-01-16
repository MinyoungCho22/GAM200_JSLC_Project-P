// Rooftop.hpp

#pragma once
#include "Background.hpp"
#include "PulseSource.hpp"
#include "DroneManager.hpp"
#include "../Engine/Vec2.hpp"
#include "../Engine/Input.hpp" 
#include <memory>
#include <vector>
#include <string>

class Shader;
class Player;
class DebugRenderer;

/**
 * @class Rooftop
 * @brief Manages the rooftop level environment, including interactive lifts, 
 * environmental hazards (abyss), and enemy drone spawning.
 */
class Rooftop
{
public:
    // Level boundary and coordinate constants
    static constexpr float WIDTH = 8400.0f;
    static constexpr float HEIGHT = 1080.0f;
    static constexpr float MIN_X = 8130.0f;
    static constexpr float MIN_Y = 1080.0f;

    void Initialize();
    void Update(double dt, Player& player, Math::Vec2 playerHitboxSize, Input::Input& input);
    void Draw(Shader& shader) const;
    void DrawRadars(const Shader& colorShader, DebugRenderer& debugRenderer) const;
    void DrawGauges(Shader& colorShader, DebugRenderer& debugRenderer) const;
    void Shutdown();
    void DrawDebug(Shader& colorShader, DebugRenderer& debugRenderer) const;

    // Entity accessors
    const std::vector<PulseSource>& GetPulseSources() const { return m_pulseSources; }
    std::vector<PulseSource>& GetPulseSources() { return m_pulseSources; }
    const std::vector<Drone>& GetDrones() const;
    std::vector<Drone>& GetDrones();
    void ClearAllDrones();

    // Interaction state queries
    bool IsPlayerCloseToHole() const { return m_isPlayerClose; }
    bool IsPlayerNearLift() const { return m_isPlayerNearLift; }
    std::string GetLiftCountdownText() const;

private:
    enum class LiftState
    {
        Idle,           // Lift is stationary
        Countdown,      // Countdown before starting movement
        Moving,         // Moving toward destination
        AtDestination   // Movement complete
    };

    std::unique_ptr<Background> m_background;
    std::unique_ptr<Background> m_closeBackground; // Visual state for when the rooftop hole is closed
    std::unique_ptr<Background> m_lift;

    // Lift transformation and state variables
    Math::Vec2 m_liftPos;
    Math::Vec2 m_liftSize;
    float m_liftInitialX = 0.0f;

    LiftState m_liftState = LiftState::Idle;
    float m_liftTargetX = 0.0f;
    float m_liftSpeed = 250.0f;
    float m_liftCountdown = 3.0f;
    float m_liftDirection = 1.0f; // 1.0 for right, -1.0 for left
    bool m_isPlayerNearLift = false;
    bool m_isLiftActivated = false; // Whether the player is currently riding the lift

    Math::Vec2 m_position;
    Math::Vec2 m_size;

    std::unique_ptr<DroneManager> m_droneManager;
    std::vector<PulseSource> m_pulseSources;

    // Environmental interaction areas (e.g., Rooftop hole)
    Math::Vec2 m_debugBoxPos;
    Math::Vec2 m_debugBoxSize;

    bool m_isClose = false;        // Whether the hole is permanently closed via interaction
    bool m_isPlayerClose = false;  // Whether the player is near the hole trigger area
};
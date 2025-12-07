//Hallway.hpp

#pragma once
#include "../Engine/Vec2.hpp"
#include "PulseSource.hpp"
#include "DroneManager.hpp"
#include <memory>
#include <vector>

class Background;
class Shader;
class Player;
class DebugRenderer;

class Hallway
{
public:

    struct HidingSpot
    {
        Math::Vec2 pos;
        Math::Vec2 size;
    };

    static constexpr float WIDTH = 5940.0f;
    static constexpr float HEIGHT = 1080.0f;

    void Initialize();
    void Update(double dt, Math::Vec2 playerCenter, Math::Vec2 playerHitboxSize, Player& player, bool isPlayerHiding);
    void Draw(Shader& shader);
    void DrawRadars(const Shader& colorShader, DebugRenderer& debugRenderer) const;
    void DrawGauges(Shader& colorShader, DebugRenderer& debugRenderer) const;
    void Shutdown();
    void DrawDebug(Shader& colorShader, DebugRenderer& debugRenderer) const;

    void ClearAllDrones();

    Math::Vec2 GetPosition() const;
    Math::Vec2 GetSize() const;

    const std::vector<PulseSource>& GetPulseSources() const;
    std::vector<PulseSource>& GetPulseSources();
    const std::vector<Drone>& GetDrones() const;
    std::vector<Drone>& GetDrones();

    const std::vector<HidingSpot>& GetHidingSpots() const;
    bool IsPlayerHiding(Math::Vec2 playerPos, Math::Vec2 playerHitboxSize, bool isPlayerCrouching) const;

private:
    std::unique_ptr<Background> m_background;
    Math::Vec2 m_position;
    Math::Vec2 m_size;
    std::vector<PulseSource> m_pulseSources;
    std::unique_ptr<DroneManager> m_droneManager;
    std::vector<HidingSpot> m_hidingSpots;

    Math::Vec2 m_obstaclePos;
    Math::Vec2 m_obstacleSize;
};
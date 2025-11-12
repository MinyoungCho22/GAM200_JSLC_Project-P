// Hallway.hpp
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
    static constexpr float WIDTH = 5940.0f;
    static constexpr float HEIGHT = 1080.0f;

    void Initialize();
    void Update(double dt, Math::Vec2 playerCenter, Math::Vec2 playerHitboxSize, Player& player, bool isPressingE);
    void Draw(Shader& shader);
    void DrawRadars(const Shader& colorShader, DebugRenderer& debugRenderer) const;
    void Shutdown();

    Math::Vec2 GetPosition() const { return m_position; }
    Math::Vec2 GetSize() const { return m_size; }

    const std::vector<PulseSource>& GetPulseSources() const { return m_pulseSources; }
    const std::vector<Drone>& GetDrones() const;
    std::vector<Drone>& GetDrones();
    void AttackDrone(Math::Vec2 playerPos, float attackRangeSq, Player& player);

private:
    std::unique_ptr<Background> m_background;
    Math::Vec2 m_position;
    Math::Vec2 m_size;
    std::vector<PulseSource> m_pulseSources;
    std::unique_ptr<DroneManager> m_droneManager;
};
//Hallway.hpp

#pragma once
#include "../Engine/Vec2.hpp"
#include "PulseSource.hpp"
#include "DroneManager.hpp"
#include "Background.hpp"
#include <memory>
#include <vector>

class Shader;
class Player;
class DebugRenderer;
struct HallwayObjectConfig;

class Hallway
{
public:

    struct HidingSpot
    {
        Math::Vec2 pos;
        Math::Vec2 size;
        std::unique_ptr<Background> sprite; // PNG sprite (nullptr when absent)
    };

    static constexpr float WIDTH = 5940.0f;
    static constexpr float HEIGHT = 1080.0f;

    void Initialize();
    void ApplyConfig(const HallwayObjectConfig& cfg);
    void Update(double dt, Math::Vec2 playerCenter, Math::Vec2 playerHitboxSize, Player& player, bool isPlayerHiding);

    void Draw(Shader& shader);
    void DrawDrones(Shader& shader);

    void DrawForeground(Shader& shader);

    void DrawRadars(const Shader& colorShader, DebugRenderer& debugRenderer) const;
    void DrawGauges(Shader& colorShader, DebugRenderer& debugRenderer) const;
    void Shutdown();
    void DrawDebug(Shader& colorShader, DebugRenderer& debugRenderer) const;

    // Draw pixel-perfect outlines when the player is near sprite-backed objects.
    // outlineShader: shader built from simple.vert + outline.frag.
    // proximityDist: reaction distance in world pixels (default: 300).
    void DrawSpriteOutlines(Shader& outlineShader,
                            Math::Vec2 playerPos, float proximityDist = 300.f) const;

    void ClearAllDrones();

    Math::Vec2 GetPosition() const;
    Math::Vec2 GetSize() const;

    const std::vector<PulseSource>& GetPulseSources() const;
    std::vector<PulseSource>& GetPulseSources();
    /// 게임 오버 후 이 구역 체크포인트로 리스폰 시 펄스 충전소를 다시 사용 가능하게 함
    void RefillPulseSourcesAfterCheckpointRespawn();
    const std::vector<Drone>& GetDrones() const;
    std::vector<Drone>& GetDrones();
    DroneManager* GetDroneManager() { return m_droneManager.get(); }

    const std::vector<HidingSpot>& GetHidingSpots() const;
    bool IsPlayerHiding(Math::Vec2 playerPos, Math::Vec2 playerHitboxSize, bool isPlayerCrouching) const;

    Math::Vec2 GetObstacleCenter() const { return m_obstaclePos; }
    Math::Vec2 GetObstacleSize() const { return m_obstacleSize; }

private:
    std::unique_ptr<Background> m_background;

    std::unique_ptr<Background> m_railing;

    Math::Vec2 m_position;
    Math::Vec2 m_size;
    std::vector<PulseSource> m_pulseSources;
    std::unique_ptr<DroneManager> m_droneManager;
    std::vector<HidingSpot> m_hidingSpots;

    Math::Vec2 m_obstaclePos;
    Math::Vec2 m_obstacleSize;
};
#pragma once
#include "../Engine/Vec2.hpp"
#include <memory>

class Player;
class Shader;

enum class DoorType
{
    RoomToHallway,
    HallwayToRooftop
};

class Door
{
public:
    void Initialize(Math::Vec2 position, Math::Vec2 size, float pulseCost, DoorType type);
    void Update(Player& player, bool isInteractKeyPressed, bool canProceed = true);
    void DrawDebug(Shader& shader) const;
    void Shutdown();

    bool ShouldLoadNextMap() const { return m_shouldLoadNextMap; }
    void ResetMapTransition() { m_shouldLoadNextMap = false; }

    bool IsPlayerNearby() const { return m_isPlayerNearby; }
    bool IsOpened() const { return m_isOpened; }

private:
    Math::Vec2 m_position{};
    Math::Vec2 m_size{};
    float m_pulseCost = 0.0f;
    DoorType m_doorType = DoorType::RoomToHallway;
    bool m_isPlayerNearby = false;
    bool m_shouldLoadNextMap = false;
    bool m_isOpened = false;
};
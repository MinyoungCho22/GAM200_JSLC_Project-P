// Door.hpp

#pragma once
#include "../Engine/Vec2.hpp"
#include <functional>

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
    void Initialize(Math::Vec2 position, Math::Vec2 size, float pulseCost = 20.0f, DoorType type = DoorType::RoomToHallway);
    void Update(Player& player, bool isInteractKeyPressed);
    void DrawDebug(Shader& shader) const;
    void Shutdown();

    bool IsPlayerNearby() const { return m_isPlayerNearby; }
    bool ShouldLoadNextMap() const { return m_shouldLoadNextMap; }
    void ResetMapTransition() { m_shouldLoadNextMap = false; }

    Math::Vec2 GetPosition() const { return m_position; }
    Math::Vec2 GetSize() const { return m_size; }
    DoorType GetType() const { return m_doorType; }

private:
    Math::Vec2 m_position{};
    Math::Vec2 m_size{};
    float m_pulseCost = 20.0f;
    bool m_isPlayerNearby = false;
    bool m_shouldLoadNextMap = false;
    DoorType m_doorType = DoorType::RoomToHallway;

    unsigned int VAO = 0;
    unsigned int VBO = 0;
};
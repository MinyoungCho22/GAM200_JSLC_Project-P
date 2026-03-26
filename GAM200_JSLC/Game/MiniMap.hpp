#pragma once

#include "../Engine/Vec2.hpp"
#include "../Engine/Matrix.hpp"
#include "Font.hpp"

class Shader;
class DebugRenderer;
class Player;
class Room;
class Hallway;
class Rooftop;
class Underground;
class Subway;
class DroneManager;

class MiniMap
{
public:
    void Draw(Shader& textureShader, Shader& colorShader, Shader& fontShader, Font& font, DebugRenderer& debugRenderer,
              const Player& player, Room& room, Hallway& hallway, Rooftop& rooftop,
              Underground& underground, Subway& subway, DroneManager& mainDroneManager,
              bool undergroundAccessed, bool subwayAccessed, const Math::Matrix& baseProjection);

private:
    struct EnemyFocus
    {
        bool valid = false;
        Math::Vec2 pos{};
        float distSq = 0.0f;
    };

    EnemyFocus FindClosestMovingEnemy(const Math::Vec2& playerPos, Hallway& hallway, Rooftop& rooftop,
                                      Underground& underground, Subway& subway,
                                      DroneManager& mainDroneManager, bool undergroundAccessed,
                                      bool subwayAccessed) const;
    std::string GetCurrentMapName(const Math::Vec2& playerPos) const;

    bool m_hasSmoothedCenter = false;
    Math::Vec2 m_smoothedCenter{};
};

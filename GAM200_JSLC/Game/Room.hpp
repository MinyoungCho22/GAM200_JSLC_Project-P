// Room.hpp

#pragma once
#include <memory>
#include "../Engine/Vec2.hpp"
#include "../Engine/Rect.hpp"
#include "Background.hpp"

class Player;
class Shader;
class Engine;
class DebugRenderer;

namespace Math { class Matrix; }

class Room
{
public:
    Room() = default;
    void Initialize(Engine& engine, const char* texturePath);
    void Shutdown();
    void Update(Player& player);
    void Draw(Engine& engine, Shader& textureShader, const Math::Matrix& projection);
    void DrawDebug(DebugRenderer& renderer, Shader& colorShader, const Math::Matrix& projection);

    void SetRightBoundaryActive(bool active) { m_rightBoundaryActive = active; }
    Math::Rect GetBoundaries() const { return m_boundaries; }
    Background* GetBackground() { return m_background.get(); }

private:
    std::unique_ptr<Background> m_background;
    Math::Rect m_boundaries;
    Math::Vec2 m_roomSize;
    Math::Vec2 m_roomCenter;
    bool m_rightBoundaryActive = true;
};
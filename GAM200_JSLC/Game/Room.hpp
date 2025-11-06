#pragma once
#include <memory>
#include "../Engine/Vec2.hpp"
#include "../Engine/Rect.hpp"
#include "Background.hpp"
#include "Player.hpp"
#include "../Engine/DebugRenderer.hpp"

class Shader;
class Engine; // Engine의 전방 선언

class Room
{
public:
    Room() = default;

    // ✅ Initialize에서 Engine 참조 제거
    void Initialize(const char* texturePath);

    void Shutdown();

    // ✅ Update에서 Engine 참조 제거 (경계가 고정되었으므로)
    void Update(Player& player);

    void Draw(Engine& engine, Shader& textureShader, const Math::Matrix& projection);
    void DrawDebug(DebugRenderer& renderer, Shader& colorShader, const Math::Matrix& projection);

private:
    std::unique_ptr<Background> m_background;
    Math::Rect m_boundaries;
    Math::Vec2 m_roomSize;
    Math::Vec2 m_roomCenter;
};
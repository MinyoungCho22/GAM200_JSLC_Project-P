#pragma once
#pragma once
#include <memory>
#include "../Engine/Vec2.hpp"
#include "../Engine/Rect.hpp"
#include "Background.hpp"
#include "Player.hpp"
#include "../Engine/DebugRenderer.hpp"

class Shader;
class Engine;

class Room
{
public:
    Room() = default;

    // 엔진을 참조하여 방의 크기와 경계를 설정하고, 배경 이미지를 로드합니다.
    void Initialize(Engine& engine, const char* texturePath);

    // 배경 리소스를 해제합니다.
    void Shutdown();

    // 플레이어의 위치를 받아 방의 경계 밖으로 나가지 못하게 제한합니다.
    void Update(Player& player);

    // 배경 이미지를 그립니다.
    void Draw(Engine& engine, Shader& textureShader, const Math::Matrix& projection);

    // 디버그 모드일 때 방의 경계를 그립니다.
    void DrawDebug(DebugRenderer& renderer, Shader& colorShader, const Math::Matrix& projection);

private:
    std::unique_ptr<Background> m_background;
    Math::Rect m_boundaries; // 방의 실제 경계 (minX, minY, maxX, maxY)
    Math::Vec2 m_roomSize;
    Math::Vec2 m_roomCenter;
};
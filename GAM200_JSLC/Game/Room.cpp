// Room.cpp

#include "Room.hpp"
#include "../Engine/Engine.hpp"
#include "../OpenGL/Shader.hpp"

constexpr float ROOM_WIDTH = 1620.0f;
constexpr float ROOM_HEIGHT = 780.0f;
constexpr float GROUND_LEVEL = 150.0f;
constexpr float GAME_WIDTH = 1920.0f;
constexpr float GAME_HEIGHT = 1080.0f;

void Room::Initialize(Engine& engine, const char* texturePath)
{
    m_background = std::make_unique<Background>();
    m_background->Initialize(texturePath);

    float screenWidth = GAME_WIDTH;

    float minX = (screenWidth - ROOM_WIDTH) / 2.0f;
    float maxX = minX + ROOM_WIDTH;
    float minY = GROUND_LEVEL;
    float maxY = minY + ROOM_HEIGHT;

    m_boundaries = { {minX, minY}, {maxX, maxY} };
    m_roomSize = { ROOM_WIDTH, ROOM_HEIGHT };
    m_roomCenter = { minX + ROOM_WIDTH / 2.0f, minY + ROOM_HEIGHT / 2.0f };
}

void Room::Shutdown()
{
    if (m_background)
    {
        m_background->Shutdown();
    }
}

void Room::Update(Player& player)
{
    Math::Vec2 centerPos = player.GetPosition();
    Math::Vec2 halfSize = player.GetSize() * 0.5f;

    if (centerPos.x - halfSize.x < m_boundaries.bottom_left.x)
    {
        player.SetPosition({ m_boundaries.bottom_left.x + halfSize.x, centerPos.y });
    }
    else if (centerPos.x + halfSize.x > m_boundaries.top_right.x)
    {
        player.SetPosition({ m_boundaries.top_right.x - halfSize.x, centerPos.y });
    }

    if (centerPos.y + halfSize.y > m_boundaries.top_right.y)
    {
        player.SetPosition({ centerPos.x, m_boundaries.top_right.y - halfSize.y });
    }
}

void Room::Draw(Engine& engine, Shader& textureShader, const Math::Matrix& projection)
{
    textureShader.use();
    textureShader.setMat4("projection", projection);

    Math::Vec2 screenSize = { GAME_WIDTH, GAME_HEIGHT };
    Math::Vec2 screenCenter = screenSize * 0.5f;
    Math::Matrix bg_model = Math::Matrix::CreateTranslation(screenCenter) * Math::Matrix::CreateScale(screenSize);
    m_background->Draw(textureShader, bg_model);
}

void Room::DrawDebug(DebugRenderer& renderer, Shader& colorShader, const Math::Matrix& projection)
{
    colorShader.use();
    colorShader.setMat4("projection", projection);
    renderer.DrawBox(colorShader, m_roomCenter, m_roomSize, { 1.0f, 0.0f });
}
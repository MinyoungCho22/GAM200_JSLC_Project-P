#include "Room.hpp"
#include "../Engine/Engine.hpp"
#include "../OpenGL/Shader.hpp"

// --- 방의 크기와 바닥 위치를 여기서 정의 ---
constexpr float ROOM_WIDTH = 1620.0f;
constexpr float ROOM_HEIGHT = 660.0f;
constexpr float GROUND_LEVEL = 170.0f; // Player.cpp와 반드시 일치해야 함(팀원 모두 주의 할 것)

void Room::Initialize(Engine& engine, const char* texturePath)
{
    m_background = std::make_unique<Background>();
    m_background->Initialize(texturePath);

    float screenWidth = static_cast<float>(engine.GetWidth());

    // 방의 절대 좌표 경계를 계산하고 저장
    float minX = (screenWidth - ROOM_WIDTH) / 2.0f;
    float maxX = minX + ROOM_WIDTH;
    float minY = GROUND_LEVEL;
    float maxY = minY + ROOM_HEIGHT;

    m_boundaries = { {minX, minY}, {maxX, maxY} };

    // 디버그 드로잉을 위해 크기와 중심점도 저장
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
    // GameplayState에 있던 플레이어 경계 제한 로직
    Math::Vec2 currentPlayerPos = player.GetPosition();
    Math::Vec2 playerSize = player.GetSize();

    // X축 경계 체크
    if (currentPlayerPos.x < m_boundaries.bottom_left.x)
    {
        player.SetPosition({ m_boundaries.bottom_left.x, currentPlayerPos.y });
    }
    else if (currentPlayerPos.x + playerSize.x > m_boundaries.top_right.x)
    {
        player.SetPosition({ m_boundaries.top_right.x - playerSize.x, currentPlayerPos.y });
    }

    // Y축 경계 체크 (천장)
    if (currentPlayerPos.y + playerSize.y > m_boundaries.top_right.y)
    {
        player.SetPosition({ currentPlayerPos.x, m_boundaries.top_right.y - playerSize.y });
    }
}

void Room::Draw(Engine& engine, Shader& textureShader, const Math::Matrix& projection)
{
    // GameplayState에 있던 배경 그리기 로직
    textureShader.use();
    textureShader.setMat4("projection", projection);

    Math::Vec2 screenSize = { (float)engine.GetWidth(), (float)engine.GetHeight() };
    Math::Vec2 screenCenter = screenSize * 0.5f;
    Math::Matrix bg_model = Math::Matrix::CreateTranslation(screenCenter) * Math::Matrix::CreateScale(screenSize);

    m_background->Draw(textureShader, bg_model);
}

void Room::DrawDebug(DebugRenderer& renderer, Shader& colorShader, const Math::Matrix& projection)
{
    // GameplayState에 있던 방 경계 디버그 그리기 로직
    colorShader.use();
    colorShader.setMat4("projection", projection);

    renderer.DrawBox(colorShader, m_roomCenter, m_roomSize, { 1.0f, 0.0f });
}
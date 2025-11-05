#include "Room.hpp"
#include "../Engine/Engine.hpp"
#include "../OpenGL/Shader.hpp"

// --- [수정] 방의 크기와 바닥 위치를 여기서 정의합니다 ---
constexpr float ROOM_WIDTH = 1620.0f;
constexpr float ROOM_HEIGHT = 660.0f;
// ✅ [수정] top-left (180, 870)에 맞게 GROUND_LEVEL(minY)을 210.0f로 변경
constexpr float GROUND_LEVEL = 210.0f;

void Room::Initialize(Engine& engine, const char* texturePath)
{
    m_background = std::make_unique<Background>();
    m_background->Initialize(texturePath);

    float screenWidth = static_cast<float>(engine.GetWidth());

    // 방의 절대 좌표 경계를 계산하고 저장합니다.
    // ✅ [수정] minX가 (1980 - 1620) / 2 = 180.0f로, 요청과 일치합니다.
    float minX = (screenWidth - ROOM_WIDTH) / 2.0f;
    float maxX = minX + ROOM_WIDTH;
    float minY = GROUND_LEVEL;
    // ✅ [수정] maxY가 210.0f + 660.0f = 870.0f로, 요청과 일치합니다.
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
    textureShader.use();
    textureShader.setMat4("projection", projection);

    Math::Vec2 screenSize = { (float)engine.GetWidth(), (float)engine.GetHeight() };
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
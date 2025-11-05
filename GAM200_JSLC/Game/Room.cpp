#include "Room.hpp"
#include "../Engine/Engine.hpp"
#include "../OpenGL/Shader.hpp"

// 방의 크기와 바닥 위치를 여기서 정의
constexpr float ROOM_WIDTH = 1620.0f;
constexpr float ROOM_HEIGHT = 660.0f;
// 방의 '시각적' 바닥(빨간 박스)을 210.0f로 설정 (870 - 660 = 210)
constexpr float GROUND_LEVEL = 210.0f;

void Room::Initialize(Engine& engine, const char* texturePath)
{
    m_background = std::make_unique<Background>();
    m_background->Initialize(texturePath);

    float screenWidth = static_cast<float>(engine.GetWidth()); // 1980

    // 방의 절대 좌표 경계를 계산하고 저장합니다.
    float minX = (screenWidth - ROOM_WIDTH) / 2.0f; // 180
    float maxX = minX + ROOM_WIDTH; // 1800
    float minY = GROUND_LEVEL; // 210
    float maxY = minY + ROOM_HEIGHT; // 210 + 660 = 870

    // 경계: X는 180~1800, Y는 210~870. 요청하신 값과 일치합니다.
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

    // X축 경계 체크
    if (centerPos.x - halfSize.x < m_boundaries.bottom_left.x)
    {
        player.SetPosition({ m_boundaries.bottom_left.x + halfSize.x, centerPos.y });
    }
    else if (centerPos.x + halfSize.x > m_boundaries.top_right.x)
    {
        player.SetPosition({ m_boundaries.top_right.x - halfSize.x, centerPos.y });
    }

    // Y축 경계 체크 (천장)
    if (centerPos.y + halfSize.y > m_boundaries.top_right.y)
    {
        player.SetPosition({ centerPos.x, m_boundaries.top_right.y - halfSize.y });
    }

    // 플레이어의 Y축 바닥 경계는 Player.cpp (230)에서 처리하므로
    // Room.cpp (210)에서는 Y축 바닥 경계를 체크할 필요가 없음. (Player가 230 위에서 멈추기 때문)
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

    // 이 함수는 Room의 'GROUND_LEVEL = 210.0f' 기준으로 계산된 m_roomCenter를 사용하므로
    // 항상 고정된 (180, 870) 위치에 빨간 박스를 그리게 됨.
    renderer.DrawBox(colorShader, m_roomCenter, m_roomSize, { 1.0f, 0.0f });
}
#include "Room.hpp"
#include "../Engine/Engine.hpp"
#include "../OpenGL/Shader.hpp"

// --- [수정] 방의 크기와 바닥 위치를 여기서 정의합니다 ---
constexpr float ROOM_WIDTH = 1620.0f;
constexpr float ROOM_HEIGHT = 660.0f;
// ✅ top-left (180, 870)에 맞게 GROUND_LEVEL(minY)을 210.0f로 변경
constexpr float GROUND_LEVEL = 210.0f;

// ✅ [수정] Initialize에서 Engine& engine 인자 제거
void Room::Initialize(const char* texturePath)
{
    m_background = std::make_unique<Background>();
    m_background->Initialize(texturePath);

    // ✅ [수정] 동적 계산 대신 요청하신 절대 좌표 180.0f를 사용합니다.
    float minX = 180.0f;
    float maxX = minX + ROOM_WIDTH; // 180 + 1620 = 1800
    float minY = GROUND_LEVEL; // 210
    float maxY = minY + ROOM_HEIGHT; // 210 + 660 = 870

    // 경계: X는 180~1800, Y는 210~870.
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

// ✅ [수정] Update에서 Engine& engine 인자 제거
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
}

void Room::Draw(Engine& engine, Shader& textureShader, const Math::Matrix& projection)
{
    textureShader.use();
    textureShader.setMat4("projection", projection);

    Math::Vec2 screenSize = { (float)engine.GetWidth(), (float)engine.GetHeight() };
    Math::Vec2 screenCenter = screenSize * 0.5f;

    Math::Vec2 imageSize = m_background->GetImageSize();
    Math::Vec2 newScaleSize = screenSize;

    if (imageSize.x > 0.0f && imageSize.y > 0.0f)
    {
        float screenAspect = screenSize.x / screenSize.y;
        float imageAspect = imageSize.x / imageSize.y;

        if (screenAspect > imageAspect)
        {
            newScaleSize.y = screenSize.y;
            newScaleSize.x = screenSize.y * imageAspect;
        }
        else
        {
            newScaleSize.x = screenSize.x;
            newScaleSize.y = screenSize.x / imageAspect;
        }
    }

    Math::Matrix bg_model = Math::Matrix::CreateTranslation(screenCenter) * Math::Matrix::CreateScale(newScaleSize);

    m_background->Draw(textureShader, bg_model);
}

void Room::DrawDebug(DebugRenderer& renderer, Shader& colorShader, const Math::Matrix& projection)
{
    colorShader.use();
    colorShader.setMat4("projection", projection);

    renderer.DrawBox(colorShader, m_roomCenter, m_roomSize, { 1.0f, 0.0f });
}
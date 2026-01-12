#include "Room.hpp"
#include "../Engine/Engine.hpp"
#include "../OpenGL/Shader.hpp"
#include "Player.hpp"
#include "../Engine/DebugRenderer.hpp"
#include "../Engine/Collision.hpp"
#include "../Game/PulseCore.hpp"

constexpr float ROOM_WIDTH = 1620.0f;
constexpr float ROOM_HEIGHT = 780.0f;
constexpr float GROUND_LEVEL = 150.0f;
constexpr float GAME_WIDTH = 1920.0f;
constexpr float GAME_HEIGHT = 1080.0f;

void Room::Initialize(Engine& engine, const char* texturePath)
{
    m_background = std::make_unique<Background>();
    m_background->Initialize(texturePath);

    m_brightBackground = std::make_unique<Background>();
    m_brightBackground->Initialize("Asset/Room_Bright.png");

    float screenWidth = GAME_WIDTH;
    float minX = (screenWidth - ROOM_WIDTH) / 2.0f;
    float maxX = minX + ROOM_WIDTH;
    float minY = GROUND_LEVEL;
    float maxY = minY + ROOM_HEIGHT;

    m_boundaries.bottom_left = Math::Vec2(minX, minY);
    m_boundaries.top_right = Math::Vec2(maxX, maxY);
    m_roomSize = { ROOM_WIDTH, ROOM_HEIGHT };
    m_roomCenter = { minX + ROOM_WIDTH / 2.0f, minY + ROOM_HEIGHT / 2.0f };

    // 콘센트 (펄스 공급원)
    float width1 = 51.f;
    float height1 = 63.f;
    float topLeftX1 = 424.f;
    float topLeftY1 = 360.f;
    Math::Vec2 center1 = { topLeftX1 + (width1 / 2.0f), topLeftY1 - (height1 / 2.0f) };
    m_pulseSources.emplace_back();
    m_pulseSources.back().Initialize(center1, { width1, height1 }, 100.f);

    // TV (펄스 주입 단서로 변경 - 펄스 공급원에서 제거)
    float width2 = 215.f;
    float height2 = 180.f;
    float topLeftX2 = 692.f;
    float topLeftY2 = 550.f;
    float tvBottomY = GAME_HEIGHT - topLeftY2;
    m_tvPos = { topLeftX2 + (width2 / 2.0f), tvBottomY - (height2 / 2.0f) };
    m_tvSize = { width2, height2 };
    m_isTVActivated = false;
    m_playerInTVArea = false;

    // 휴대폰 (펄스 공급원)
    float width3 = 75.f;
    float height3 = 33.f;
    float topLeftX3 = 1414.f;
    float topLeftY3 = 212.f;
    Math::Vec2 center3 = { topLeftX3 + (width3 / 2.0f), topLeftY3 - (height3 / 2.0f) };
    m_pulseSources.emplace_back();
    m_pulseSources.back().Initialize(center3, { width3, height3 }, 100.f);

    float blindWidth = 310.f;
    float blindHeight = 300.f;
    float blindTopLeftX = 1105.f;
    float blindTopLeftY = 352.f;
    float blindBottomY = GAME_HEIGHT - blindTopLeftY;

    m_blindPos = { blindTopLeftX + (blindWidth / 2.0f), blindBottomY - (blindHeight / 2.0f) };
    m_blindSize = { blindWidth, blindHeight };
    m_isBright = false;
    m_playerInBlindArea = false;
}

void Room::Shutdown()
{
    if (m_background)
    {
        m_background->Shutdown();
    }
    if (m_brightBackground)
    {
        m_brightBackground->Shutdown();
    }
    for (auto& source : m_pulseSources)
    {
        source.Shutdown();
    }
}

void Room::Update(Player& player, double dt, Input::Input& input)
{
    Math::Vec2 centerPos = player.GetPosition();
    Math::Vec2 halfSize = player.GetSize() * 0.5f;

    if (centerPos.x - halfSize.x < m_boundaries.bottom_left.x)
    {
        player.SetPosition({ m_boundaries.bottom_left.x + halfSize.x, centerPos.y });
    }
    else if (m_rightBoundaryActive && centerPos.x + halfSize.x > m_boundaries.top_right.x)
    {
        player.SetPosition({ m_boundaries.top_right.x - halfSize.x, centerPos.y });
    }

    if (centerPos.y + halfSize.y > m_boundaries.top_right.y)
    {
        player.SetPosition({ centerPos.x, m_boundaries.top_right.y - halfSize.y });
    }

    m_playerInBlindArea = Collision::CheckAABB(player.GetPosition(), player.GetHitboxSize(), m_blindPos, m_blindSize);
    m_playerInTVArea = Collision::CheckAABB(player.GetPosition(), player.GetHitboxSize(), m_tvPos, m_tvSize);

    // 블라인드 상호작용 (펄스 주입 단서)
    if (m_playerInBlindArea && input.IsKeyTriggered(Input::Key::J) && !m_isBright)
    {
        const float BLIND_TOGGLE_COST = 20.0f;
        Pulse& pulse = player.GetPulseCore().getPulse();

        if (pulse.Value() >= BLIND_TOGGLE_COST)
        {
            pulse.spend(BLIND_TOGGLE_COST);
            m_isBright = true;
        }
    }

    // TV 상호작용 (펄스 주입 단서)
    if (m_playerInTVArea && input.IsKeyTriggered(Input::Key::J) && !m_isTVActivated)
    {
        const float TV_ACTIVATE_COST = 20.0f;
        Pulse& pulse = player.GetPulseCore().getPulse();

        if (pulse.Value() >= TV_ACTIVATE_COST)
        {
            pulse.spend(TV_ACTIVATE_COST);
            m_isTVActivated = true;
        }
    }
}

void Room::Draw(Shader& textureShader) const
{
    Math::Vec2 screenSize = { GAME_WIDTH, GAME_HEIGHT };
    Math::Vec2 screenCenter = screenSize * 0.5f;
    Math::Matrix bg_model = Math::Matrix::CreateTranslation(screenCenter) * Math::Matrix::CreateScale(screenSize);

    textureShader.setMat4("model", bg_model);

    if (m_isBright)
    {
        m_brightBackground->Draw(textureShader, bg_model);
    }
    else
    {
        m_background->Draw(textureShader, bg_model);
    }
}

void Room::DrawDebug(DebugRenderer& renderer, Shader& colorShader, const Math::Matrix& projection, const Player& player) const
{
    colorShader.use();
    colorShader.setMat4("projection", projection);

    renderer.DrawBox(colorShader, m_roomCenter, m_roomSize, { 1.0f, 0.0f });

    for (const auto& source : m_pulseSources)
    {
        renderer.DrawBox(colorShader, source.GetPosition(), source.GetSize(), { 1.0f, 0.5f });
    }

    Math::Vec2 blindDebugColor = m_playerInBlindArea ? Math::Vec2(1.0f, 1.0f) : Math::Vec2(0.5f, 1.0f);
    renderer.DrawBox(colorShader, m_blindPos, m_blindSize, blindDebugColor);

    Math::Vec2 tvDebugColor = m_playerInTVArea ? Math::Vec2(1.0f, 0.0f) : Math::Vec2(0.5f, 0.0f);
    renderer.DrawBox(colorShader, m_tvPos, m_tvSize, tvDebugColor);
}

bool Room::IsPlayerHiding(Math::Vec2 playerPos, Math::Vec2 playerHitboxSize, bool isPlayerCrouching) const
{
    if (!isPlayerCrouching)
    {
        return false;
    }

    return Collision::CheckAABB(playerPos, playerHitboxSize, m_blindPos, m_blindSize);
}
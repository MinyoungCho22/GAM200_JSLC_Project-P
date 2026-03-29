// Room.cpp

#include "Room.hpp"
#include "../Engine/Engine.hpp"
#include "../OpenGL/Shader.hpp"
#include "Player.hpp"
#include "../Engine/DebugRenderer.hpp"
#include "../Engine/Collision.hpp"
#include "../Game/PulseCore.hpp"
#include "MapObjectConfig.hpp"

// Level design constants
constexpr float ROOM_WIDTH = 1620.0f;
constexpr float ROOM_HEIGHT = 780.0f;
constexpr float GROUND_LEVEL = 150.0f;
constexpr float GAME_WIDTH = 1920.0f;
constexpr float GAME_HEIGHT = 1080.0f;

void Room::Initialize(Engine& engine, const char* texturePath)
{
    // Load backgrounds for both lighting states
    m_background = std::make_unique<Background>();
    m_background->Initialize(texturePath);

    m_brightBackground = std::make_unique<Background>();
    m_brightBackground->Initialize("Asset/Room_Bright.png");

    // Define room boundaries relative to screen center
    float screenWidth = GAME_WIDTH;
    float minX = (screenWidth - ROOM_WIDTH) / 2.0f;
    float maxX = minX + ROOM_WIDTH;
    float minY = GROUND_LEVEL;
    float maxY = minY + ROOM_HEIGHT;

    m_boundaries.bottom_left = Math::Vec2(minX, minY);
    m_boundaries.top_right = Math::Vec2(maxX, maxY);
    m_roomSize = { ROOM_WIDTH, ROOM_HEIGHT };
    m_roomCenter = { minX + ROOM_WIDTH / 2.0f, minY + ROOM_HEIGHT / 2.0f };

    ApplyConfig(MapObjectConfig::Instance().GetData().room);
    
    m_isBright = false;
    m_playerInBlindArea = false;
}

void Room::ApplyConfig(const RoomObjectConfig& cfg)
{
    for (auto& source : m_pulseSources) source.Shutdown();
    m_pulseSources.clear();

    for (const auto& p : cfg.pulseSources)
    {
        Math::Vec2 center = { p.topLeft.x + p.size.x * 0.5f, p.topLeft.y - p.size.y * 0.5f };
        m_pulseSources.emplace_back();
        m_pulseSources.back().Initialize(center, p.size, 100.0f);
    }

    float blindBottomY = GAME_HEIGHT - cfg.blind.topLeft.y;
    m_blindPos = { cfg.blind.topLeft.x + cfg.blind.size.x * 0.5f,
                   blindBottomY - cfg.blind.size.y * 0.5f };
    m_blindSize = cfg.blind.size;
}

void Room::Shutdown()
{
    if (m_background) m_background->Shutdown();
    if (m_brightBackground) m_brightBackground->Shutdown();
    for (auto& source : m_pulseSources) source.Shutdown();
}

void Room::Update(Player& player, double dt, Input::Input& input, Math::Vec2 mouseWorldPos)
{
    (void)dt;
    (void)mouseWorldPos;

    Math::Vec2 centerPos = player.GetPosition();
    Math::Vec2 halfSize = player.GetSize() * 0.5f;

    // Handle Wall Collision (Horizontal boundaries)
    if (centerPos.x - halfSize.x < m_boundaries.bottom_left.x)
    {
        player.SetPosition({ m_boundaries.bottom_left.x + halfSize.x, centerPos.y });
    }
    else if (m_rightBoundaryActive && centerPos.x + halfSize.x > m_boundaries.top_right.x)
    {
        player.SetPosition({ m_boundaries.top_right.x - halfSize.x, centerPos.y });
    }

    // Handle Ceiling Collision (Vertical boundary)
    if (centerPos.y + halfSize.y > m_boundaries.top_right.y)
    {
        player.SetPosition({ centerPos.x, m_boundaries.top_right.y - halfSize.y });
    }

    // Check for interaction with Blinds
    m_playerInBlindArea = Collision::CheckAABB(player.GetPosition(), player.GetHitboxSize(), m_blindPos, m_blindSize);

    // More forgiving interaction: if player is in blind area, left click works
    // without requiring a precise cursor-over-blind point test.
    if (m_playerInBlindArea && input.IsMouseButtonTriggered(Input::MouseButton::Left) && !m_isBright)
    {
        const float BLIND_TOGGLE_COST = 20.0f;
        const float MIN_PULSE_TO_OPERATE_BLIND = 30.0f;
        Pulse& pulse = player.GetPulseCore().getPulse();

        // Do not allow blind interaction when pulse is 30 or below.
        if (pulse.Value() > MIN_PULSE_TO_OPERATE_BLIND && pulse.Value() >= BLIND_TOGGLE_COST)
        {
            pulse.spend(BLIND_TOGGLE_COST);
            m_isBright = true;
        }
        else
        {
            m_blindInteractDenied = true;
        }
    }
}

bool Room::ConsumeBlindInteractDenied()
{
    const bool v = m_blindInteractDenied;
    m_blindInteractDenied = false;
    return v;
}

void Room::Draw(Shader& textureShader) const
{
    Math::Vec2 screenSize = { GAME_WIDTH, GAME_HEIGHT };
    Math::Vec2 screenCenter = screenSize * 0.5f;
    
    // Background fills the entire logical game area
    Math::Matrix bg_model = Math::Matrix::CreateTranslation(screenCenter) * Math::Matrix::CreateScale(screenSize);
    textureShader.setMat4("model", bg_model);

    // Render appropriate background based on blind state
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

    // Draw level boundaries (Orange)
    renderer.DrawBox(colorShader, m_roomCenter, m_roomSize, { 1.0f, 0.0f });

    // Draw PulseSource zones (Yellow/Amber)
    for (const auto& source : m_pulseSources)
    {
        renderer.DrawBox(colorShader, source.GetPosition(), source.GetSize(), { 1.0f, 0.5f });
    }

    // Draw Blind interaction zone (White if active, Light Blue otherwise)
    Math::Vec2 debugColor = m_playerInBlindArea ? Math::Vec2(1.0f, 1.0f) : Math::Vec2(0.5f, 1.0f);
    renderer.DrawBox(colorShader, m_blindPos, m_blindSize, debugColor);
}

bool Room::IsPlayerHiding(Math::Vec2 playerPos, Math::Vec2 playerHitboxSize, bool isPlayerCrouching) const
{
    // Hiding is only possible if the player is actively crouching within the zone
    if (!isPlayerCrouching)
    {
        return false;
    }

    return Collision::CheckAABB(playerPos, playerHitboxSize, m_blindPos, m_blindSize);
}
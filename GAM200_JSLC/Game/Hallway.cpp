#include "Hallway.hpp"
#include "Background.hpp"
#include "Player.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/Collision.hpp"
#include "../Engine/Logger.hpp"
#include "../Engine/DebugRenderer.hpp"

constexpr float ROOM_WIDTH = 1920.0f;

void Hallway::Initialize()
{
    m_background = std::make_unique<Background>();
    m_background->Initialize("Asset/Hallway.png");
    m_size = { WIDTH, HEIGHT };
    m_position = { ROOM_WIDTH + WIDTH / 2.0f, HEIGHT / 2.0f };

    float width1 = 210.f;
    float height1 = 312.f;
    float topLeftX1 = 3879.f;
    float topLeftY1 = 708.f;
    float bottomY1 = HEIGHT - topLeftY1;
    Math::Vec2 center1 = {
        topLeftX1 + (width1 / 2.0f),
        bottomY1 + (height1 / 2.0f)
    };
    m_pulseSources.emplace_back();
    m_pulseSources.back().Initialize(center1, { width1, height1 }, 100.f);

    float width2 = 381.f;
    float height2 = 324.f;
    float topLeftX2 = 2820.f;
    float topLeftY2 = 923.f;
    float bottomY2 = HEIGHT - topLeftY2;
    m_hidingSpotPos = {
        topLeftX2 + (width2 / 2.0f),
        bottomY2 + (height2 / 2.0f)
    };
    m_hidingSpotSize = { width2, height2 };

    m_droneManager = std::make_unique<DroneManager>();
    m_droneManager->SpawnDrone({ 2500.0f, 400.0f }, "Asset/drone.png");
}


void Hallway::Update(double dt, Math::Vec2 playerCenter, Math::Vec2 playerHitboxSize, Player& player)
{
    bool isHiding = IsPlayerHiding(playerCenter, playerHitboxSize, player.IsCrouching());

    m_droneManager->Update(dt, player, playerHitboxSize, isHiding);

    Math::Vec2 playerPos = player.GetPosition();
    Math::Vec2 playerSize = player.GetSize();

    float playerLeftEdge = playerPos.x - (playerSize.x / 2.0f);

    
}

void Hallway::Draw(Shader& shader)
{
    Math::Matrix model = Math::Matrix::CreateTranslation(m_position) * Math::Matrix::CreateScale(m_size);
    shader.setMat4("model", model);
    m_background->Draw(shader, model);

    m_droneManager->Draw(shader);
}

void Hallway::DrawRadars(const Shader& colorShader, DebugRenderer& debugRenderer) const
{
    m_droneManager->DrawRadars(colorShader, debugRenderer);
}

void Hallway::Shutdown()
{
    if (m_background)
    {
        m_background->Shutdown();
    }

    for (auto& source : m_pulseSources)
    {
        source.Shutdown();
    }

    if (m_droneManager)
    {
        m_droneManager->Shutdown();
    }
}

void Hallway::ClearAllDrones()
{
    if (m_droneManager)
    {
        m_droneManager->ClearAllDrones();
    }
}

Math::Vec2 Hallway::GetPosition() const
{
    return m_position;
}

Math::Vec2 Hallway::GetSize() const
{
    return m_size;
}

const std::vector<Drone>& Hallway::GetDrones() const
{
    return m_droneManager->GetDrones();
}

std::vector<Drone>& Hallway::GetDrones()
{
    return m_droneManager->GetDrones();
}

const std::vector<PulseSource>& Hallway::GetPulseSources() const
{
    return m_pulseSources;
}

std::vector<PulseSource>& Hallway::GetPulseSources()
{
    return m_pulseSources;
}

bool Hallway::IsPlayerHiding(Math::Vec2 playerPos, Math::Vec2 playerHitboxSize, bool isPlayerCrouching) const
{
    if (!isPlayerCrouching)
    {
        return false;
    }

    return Collision::CheckAABB(playerPos, playerHitboxSize, m_hidingSpotPos, m_hidingSpotSize);
}

void Hallway::DrawDebug(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    debugRenderer.DrawBox(colorShader, m_hidingSpotPos, m_hidingSpotSize, { 0.3f, 1.0f });
}
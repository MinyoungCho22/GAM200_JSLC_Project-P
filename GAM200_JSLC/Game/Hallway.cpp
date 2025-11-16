//Hallway.cpp

#include "Hallway.hpp"
#include "Background.hpp"
#include "Player.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/Collision.hpp"
#include "../Engine/Logger.hpp"
#include "../Engine/DebugRenderer.hpp"
#include <algorithm> 

constexpr float ROOM_WIDTH = 1920.0f;

void Hallway::Initialize()
{
    m_background = std::make_unique<Background>();
    m_background->Initialize("Asset/Hallway.png");
    m_size = { WIDTH, HEIGHT };
    m_position = { ROOM_WIDTH + WIDTH / 2.0f, HEIGHT / 2.0f };

    float width1 = 210.f;
    float height1 = 312.f;
    float topLeftX1 = 2456.f;
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

    float width3 = 369.f;
    float height3 = 645.f;
    float topLeftX3 = 7489.f;
    float topLeftY3 = 973.f;
    float bottomY3 = HEIGHT - topLeftY3;
    m_obstaclePos = {
        topLeftX3 + (width3 / 2.0f),
        bottomY3 + (height3 / 2.0f)
    };
    m_obstacleSize = { width3, height3 };


    m_droneManager = std::make_unique<DroneManager>();
    m_droneManager->SpawnDrone({ 2500.0f, 400.0f }, "Asset/drone.png");
}


void Hallway::Update(double dt, Math::Vec2 playerCenter, Math::Vec2 playerHitboxSize, Player& player)
{
    bool isHiding = IsPlayerHiding(playerCenter, playerHitboxSize, player.IsCrouching());

    m_droneManager->Update(dt, player, playerHitboxSize, isHiding);

    Math::Vec2 playerPos = player.GetPosition();

    if (Collision::CheckAABB(playerPos, playerHitboxSize, m_obstaclePos, m_obstacleSize))
    {
        Math::Vec2 playerHalfSize = playerHitboxSize / 2.0f;
        Math::Vec2 obsHalfSize = m_obstacleSize / 2.0f;

        Math::Vec2 obsMin = m_obstaclePos - obsHalfSize;
        Math::Vec2 obsMax = m_obstaclePos + obsHalfSize;
        Math::Vec2 playerMin = playerPos - playerHalfSize;
        Math::Vec2 playerMax = playerPos + playerHalfSize;

        float overlapX = std::min(playerMax.x, obsMax.x) - std::max(playerMin.x, obsMin.x);
        float overlapY = std::min(playerMax.y, obsMax.y) - std::max(playerMin.y, obsMin.y);

        Math::Vec2 newPos = playerPos;

        if (overlapX < overlapY)
        {
            if (playerPos.x < m_obstaclePos.x)
                newPos.x = obsMin.x - playerHalfSize.x;
            else
                newPos.x = obsMax.x + playerHalfSize.x;
        }
        else
        {
            if (playerPos.y < m_obstaclePos.y)
                newPos.y = obsMin.y - playerHalfSize.y;
            else
                newPos.y = obsMax.y + playerHalfSize.y;

            player.ResetVelocity();
            player.SetOnGround(true);
        }
        player.SetPosition(newPos);
    }
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
    debugRenderer.DrawBox(colorShader, m_obstaclePos, m_obstacleSize, { 1.0f, 0.0f });
}
//Hallway.cpp

#include "Hallway.hpp"
#include "Background.hpp"
#include "Player.hpp"
#include "MapObjectTypes.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/Collision.hpp"
#include "../Engine/DebugRenderer.hpp"
#include <algorithm> 
#include <cmath>

constexpr float ROOM_WIDTH = 1920.0f;

void Hallway::Initialize()
{
    m_background = std::make_unique<Background>();
    m_background->Initialize("Asset/Hallway.png");

    m_railing = std::make_unique<Background>();
    m_railing->Initialize("Asset/Railing.png");

    m_size = { WIDTH, HEIGHT };
    m_position = { ROOM_WIDTH + WIDTH / 2.0f, HEIGHT / 2.0f };

    m_droneManager = std::make_unique<DroneManager>();
    m_droneManager->SpawnDrone({ 2600.0f, 400.0f }, "Asset/Drone.png");
    m_droneManager->SpawnDrone({ 5500.0f, 400.0f }, "Asset/Drone.png");
}

void Hallway::ApplyConfig(const HallwayObjectConfig& cfg)
{
    for (auto& source : m_pulseSources) source.Shutdown();
    m_pulseSources.clear();

    for (auto& spot : m_hidingSpots)
    {
        if (spot.sprite)
        {
            spot.sprite->Shutdown();
            spot.sprite.reset();
        }
    }
    m_hidingSpots.clear();

    for (const auto& p : cfg.pulseSources)
    {
        float bottomY = HEIGHT - p.topLeft.y;
        Math::Vec2 center = { p.topLeft.x + p.size.x * 0.5f, bottomY + p.size.y * 0.5f };
        m_pulseSources.emplace_back();
        m_pulseSources.back().Initialize(center, p.size, 100.0f);
        if (!p.spritePath.empty()) m_pulseSources.back().InitializeSprite(p.spritePath.c_str());
    }

    for (const auto& h : cfg.hidingSpots)
    {
        float bottomY = HEIGHT - h.topLeft.y;
        Math::Vec2 center = { h.topLeft.x + h.size.x * 0.5f, bottomY + h.size.y * 0.5f };
        m_hidingSpots.emplace_back(HidingSpot{ center, h.size, nullptr });
        if (!h.spritePath.empty())
        {
            m_hidingSpots.back().sprite = std::make_unique<Background>();
            m_hidingSpots.back().sprite->Initialize(h.spritePath.c_str());
        }
    }

    float obsBottomY = HEIGHT - cfg.obstacle.topLeft.y;
    m_obstaclePos = { cfg.obstacle.topLeft.x + cfg.obstacle.size.x * 0.5f,
                      obsBottomY + cfg.obstacle.size.y * 0.5f };
    m_obstacleSize = cfg.obstacle.size;
}


void Hallway::Update(double dt, Math::Vec2 playerCenter, Math::Vec2 playerHitboxSize, Player& player, bool isPlayerHiding)
{
    m_droneManager->Update(dt, player, playerHitboxSize, isPlayerHiding);

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

    for (const auto& source : m_pulseSources)
    {
        source.DrawSprite(shader);
    }

    for (const auto& spot : m_hidingSpots)
    {
        if (spot.sprite)
        {
            shader.setVec4("spriteRect", 0.f, 0.f, 1.f, 1.f);
            shader.setBool("flipX", false);
            Math::Matrix spotModel = Math::Matrix::CreateTranslation(spot.pos) * Math::Matrix::CreateScale(spot.size);
            spot.sprite->Draw(shader, spotModel);
        }
    }

    m_droneManager->Draw(shader);
}


void Hallway::DrawForeground(Shader& shader)
{
    if (!m_railing) return;

    float railW = 240.0f;
    float railH = 207.0f;

    int railCount = static_cast<int>(std::ceil(WIDTH / railW));

    float startX = ROOM_WIDTH + (railW / 2.0f);
    float startY = railH / 2.0f;

    for (int i = 0; i < railCount; ++i)
    {
        Math::Vec2 railPos = { startX + (i * railW), startY };
        Math::Vec2 railSize = { railW, railH };

        Math::Matrix railModel = Math::Matrix::CreateTranslation(railPos) * Math::Matrix::CreateScale(railSize);

        shader.setMat4("model", railModel);
        m_railing->Draw(shader, railModel);
    }
}

void Hallway::DrawRadars(const Shader& colorShader, DebugRenderer& debugRenderer) const
{
    m_droneManager->DrawRadars(colorShader, debugRenderer);
}

void Hallway::DrawGauges(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    m_droneManager->DrawGauges(colorShader, debugRenderer);
}

void Hallway::Shutdown()
{
    if (m_background)
    {
        m_background->Shutdown();
    }

    if (m_railing)
    {
        m_railing->Shutdown();
    }

    for (auto& source : m_pulseSources)
    {
        source.Shutdown();
    }

    for (auto& spot : m_hidingSpots)
    {
        if (spot.sprite)
        {
            spot.sprite->Shutdown();
            spot.sprite.reset();
        }
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

const std::vector<Hallway::HidingSpot>& Hallway::GetHidingSpots() const
{
    return m_hidingSpots;
}

bool Hallway::IsPlayerHiding(Math::Vec2 playerPos, Math::Vec2 playerHitboxSize, bool isPlayerCrouching) const
{
    if (!isPlayerCrouching)
    {
        return false;
    }

    for (const auto& spot : m_hidingSpots)
    {
        if (Collision::CheckAABB(playerPos, playerHitboxSize, spot.pos, spot.size))
        {
            return true;
        }
    }

    return false;
}

void Hallway::DrawDebug(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    for (const auto& spot : m_hidingSpots)
    {
        debugRenderer.DrawBox(colorShader, spot.pos, spot.size, { 0.3f, 1.0f });
    }
    debugRenderer.DrawBox(colorShader, m_obstaclePos, m_obstacleSize, { 1.0f, 0.0f });
}

void Hallway::DrawSpriteOutlines(Shader& outlineShader,
                                  Math::Vec2 playerPos, float proximityDist) const
{
    const float proxDistSq = proximityDist * proximityDist;

    for (const auto& source : m_pulseSources)
    {
        if (!source.HasSprite()) continue;
        float distSq = (playerPos - source.GetPosition()).LengthSq();
        if (distSq <= proxDistSq)
        {
            source.DrawOutline(outlineShader);
        }
    }

    for (const auto& spot : m_hidingSpots)
    {
        if (!spot.sprite) continue;
        float distSq = (playerPos - spot.pos).LengthSq();
        if (distSq <= proxDistSq)
        {
            int w = spot.sprite->GetWidth();
            int h = spot.sprite->GetHeight();
            if (w <= 0 || h <= 0) continue;

            outlineShader.setVec2("texelSize", 1.0f / w, 1.0f / h);
            outlineShader.setVec4("outlineColor", 0.2f, 0.6f, 1.0f, 1.0f);
            outlineShader.setFloat("outlineWidthTexels", 2.0f);

            Math::Matrix model = Math::Matrix::CreateTranslation(spot.pos) * Math::Matrix::CreateScale(spot.size);
            spot.sprite->Draw(outlineShader, model);
        }
    }
}
// Subway.cpp

#include "Subway.hpp"
#include "Background.hpp"
#include "Player.hpp"
#include "DroneManager.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/DebugRenderer.hpp"
#include "../Engine/Collision.hpp"
#include "../Engine/Logger.hpp"
#include "MapObjectConfig.hpp"
#include <algorithm>


void Subway::Initialize()
{
    Logger::Instance().Log(Logger::Severity::Info, "Subway Map Initialize");

    // Load subway background image
    m_background = std::make_unique<Background>();
    m_background->Initialize("Asset/Subway.png");

    m_size = { WIDTH, HEIGHT };
    m_position = { MIN_X + WIDTH / 2.0f, MIN_Y + HEIGHT / 2.0f };

    // Initialize drone manager (no drones)
    m_droneManager = std::make_unique<DroneManager>();

    // No robot spawns in subway map
    // (Currently no robots either)

    ApplyConfig(MapObjectConfig::Instance().GetData().subway);

    Logger::Instance().Log(Logger::Severity::Info, "Subway Map initialized with %d robots and %d drones",
        m_robots.size(), m_droneManager->GetDrones().size());
}

void Subway::ApplyConfig(const SubwayObjectConfig& cfg)
{
    for (auto& source : m_pulseSources) source.Shutdown();
    m_pulseSources.clear();
    m_obstacles.clear();

    for (const auto& p : cfg.pulseSources)
    {
        float cx = MIN_X + p.topLeft.x + p.size.x * 0.5f;
        float cy = MIN_Y + (HEIGHT - p.topLeft.y) - p.size.y * 0.5f;
        m_pulseSources.emplace_back();
        m_pulseSources.back().Initialize({ cx, cy }, p.size, 100.0f);
    }

    for (const auto& o : cfg.obstacles)
    {
        float cx = MIN_X + o.topLeft.x + o.size.x * 0.5f;
        float cy = MIN_Y + (HEIGHT - o.topLeft.y) - o.size.y * 0.5f;
        m_obstacles.push_back({ {cx, cy}, o.size });
    }
}

void Subway::Update(double dt, Player& player, Math::Vec2 playerHitboxSize)
{
    Math::Vec2 playerPos = player.GetPosition();

    // Check if player is within subway map bounds
    bool isPlayerInSubway = (playerPos.y >= MIN_Y && playerPos.y <= MIN_Y + HEIGHT &&
                             playerPos.x >= MIN_X && playerPos.x <= MIN_X + WIDTH);

    if (!isPlayerInSubway)
    {
        return;
    }

    // Collect obstacle info (needed for Robot::Update)
    std::vector<ObstacleInfo> obstacleInfos;
    for (const auto& obs : m_obstacles) {
        obstacleInfos.push_back({ obs.pos, obs.size });
    }

    float mapMinX = MIN_X;
    float mapMaxX = MIN_X + WIDTH;

    // Update robots
    for (auto& robot : m_robots)
    {
        if (!robot.IsDead())
        {
            robot.Update(dt, player, obstacleInfos, mapMinX, mapMaxX);
        }
    }

    // Update drones
    m_droneManager->Update(dt, player, playerHitboxSize, false);

    // Constrain player to boundaries
    Math::Vec2 currentPlayerPos = player.GetPosition();
    Math::Vec2 playerHalfSize = playerHitboxSize * 0.5f;

    float leftBoundary = MIN_X;
    if (currentPlayerPos.x - playerHalfSize.x < leftBoundary)
    {
        player.SetPosition({ leftBoundary + playerHalfSize.x, currentPlayerPos.y });
    }

    float rightBoundary = MIN_X + WIDTH;
    if (currentPlayerPos.x + playerHalfSize.x > rightBoundary)
    {
        player.SetPosition({ rightBoundary - playerHalfSize.x, currentPlayerPos.y });
    }

    // Handle obstacle collision
    Math::Vec2 currentHitboxCenter = player.GetPosition();
    Math::Vec2 playerHalfSizeCheck = playerHitboxSize / 2.0f;

    for (const auto& obstacle : m_obstacles)
    {
        if (Collision::CheckAABB(currentHitboxCenter, playerHitboxSize, obstacle.pos, obstacle.size))
        {
            Math::Vec2 obstacleHalfSize = obstacle.size / 2.0f;
            Math::Vec2 playerMin = currentHitboxCenter - playerHalfSizeCheck;
            Math::Vec2 playerMax = currentHitboxCenter + playerHalfSizeCheck;
            Math::Vec2 obstacleMin = obstacle.pos - obstacleHalfSize;
            Math::Vec2 obstacleMax = obstacle.pos + obstacleHalfSize;

            float overlapLeft = playerMax.x - obstacleMin.x;
            float overlapRight = obstacleMax.x - playerMin.x;
            float overlapTop = playerMax.y - obstacleMin.y;
            float overlapBottom = obstacleMax.y - playerMin.y;

            float minOverlap = std::min({ overlapLeft, overlapRight, overlapTop, overlapBottom });

            if (minOverlap == overlapLeft)
            {
                Math::Vec2 shift = { -(overlapLeft), 0.0f };
                player.SetPosition(player.GetPosition() + shift);
            }
            else if (minOverlap == overlapRight)
            {
                Math::Vec2 shift = { overlapRight, 0.0f };
                player.SetPosition(player.GetPosition() + shift);
            }
            else if (minOverlap == overlapTop)
            {
                Math::Vec2 shift = { 0.0f, -(overlapTop) };
                player.SetPosition(player.GetPosition() + shift);
                player.ResetVelocity();
                player.SetOnGround(true);
            }
            else if (minOverlap == overlapBottom)
            {
                Math::Vec2 shift = { 0.0f, overlapBottom };
                player.SetPosition(player.GetPosition() + shift);
            }

            currentHitboxCenter = player.GetPosition();
        }
    }
}

void Subway::Draw(Shader& shader) const
{
    // Draw background
    Math::Matrix model = Math::Matrix::CreateTranslation(m_position) * Math::Matrix::CreateScale(m_size);
    shader.setMat4("model", model);
    m_background->Draw(shader, model);

    // Draw robots
    for (const auto& robot : m_robots)
    {
        if (!robot.IsDead())
        {
            robot.Draw(shader);
        }
    }

    // Draw drones
    m_droneManager->Draw(shader);
}

void Subway::DrawRadars(const Shader& colorShader, DebugRenderer& debugRenderer) const
{
    m_droneManager->DrawRadars(colorShader, debugRenderer);
}

void Subway::DrawGauges(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    m_droneManager->DrawGauges(colorShader, debugRenderer);

    for (const auto& robot : m_robots)
    {
        if (!robot.IsDead())
        {
            robot.DrawGauge(colorShader, debugRenderer);
        }
    }
}

void Subway::DrawDebug(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    // Pulse source debug boxes (green)
    for (const auto& source : m_pulseSources)
    {
        debugRenderer.DrawBox(colorShader, source.GetPosition(), source.GetSize(), { 0.0f, 1.0f });
    }

    // Obstacle debug boxes (red)
    for (const auto& obstacle : m_obstacles)
    {
        debugRenderer.DrawBox(colorShader, obstacle.pos, obstacle.size, { 1.0f, 0.0f });
    }

    // Map boundary debug (white)
    float leftBoundary = MIN_X;
    float rightBoundary = MIN_X + WIDTH;
    float boundaryHeight = HEIGHT;
    Math::Vec2 leftBoundaryPos = { leftBoundary, MIN_Y + boundaryHeight / 2.0f };
    Math::Vec2 rightBoundaryPos = { rightBoundary, MIN_Y + boundaryHeight / 2.0f };
    Math::Vec2 boundarySize = { 10.0f, boundaryHeight };

    debugRenderer.DrawBox(colorShader, leftBoundaryPos, boundarySize, { 1.0f, 1.0f });
    debugRenderer.DrawBox(colorShader, rightBoundaryPos, boundarySize, { 1.0f, 1.0f });
}

void Subway::Shutdown()
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

    Logger::Instance().Log(Logger::Severity::Info, "Subway Map Shutdown");
}

const std::vector<Drone>& Subway::GetDrones() const
{
    return m_droneManager->GetDrones();
}

std::vector<Drone>& Subway::GetDrones()
{
    return m_droneManager->GetDrones();
}

void Subway::ClearAllDrones()
{
    m_droneManager->ClearAllDrones();
}

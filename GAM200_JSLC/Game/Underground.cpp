//Underground.cpp

#include "Underground.hpp"
#include "Background.hpp"
#include "Player.hpp"
#include "DroneManager.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/DebugRenderer.hpp"
#include "../Engine/Collision.hpp"
#include "MapObjectConfig.hpp"
#include <algorithm>


void Underground::Initialize()
{
    // Initialize background parallax/static image
    m_background = std::make_unique<Background>();
    m_background->Initialize("Asset/Underground.png");

    m_size = { WIDTH, HEIGHT };
    m_position = { MIN_X + WIDTH / 2.0f, MIN_Y + HEIGHT / 2.0f };

    m_droneManager = std::make_unique<DroneManager>();

    // Spawn aerial drones with varying speeds
    float droneY = MIN_Y + 550.0f;
    m_droneManager->SpawnDrone({ 19423.0f, droneY }, "Asset/Drone.png", false).SetBaseSpeed(180.0f);
    m_droneManager->SpawnDrone({ 22203.0f, droneY }, "Asset/Drone.png", false).SetBaseSpeed(250.0f);
    m_droneManager->SpawnDrone({ 22787.0f, droneY }, "Asset/Drone.png", false).SetBaseSpeed(400.0f);

    ApplyConfig(MapObjectConfig::Instance().GetData().underground);
}

void Underground::ApplyConfig(const UndergroundObjectConfig& cfg)
{
    for (auto& source : m_pulseSources) source.Shutdown();
    for (auto& robot : m_robots) robot.Shutdown();
    m_pulseSources.clear();
    m_obstacles.clear();
    m_ramps.clear();
    m_robots.clear();

    for (const auto& spawn : cfg.robotSpawns)
    {
        m_robots.emplace_back();
        m_robots.back().Init(spawn);
    }

    for (const auto& o : cfg.obstacles)
    {
        float cx = MIN_X + o.topLeft.x + o.size.x * 0.5f;
        float cy = MIN_Y + (HEIGHT - o.topLeft.y) - o.size.y * 0.5f;
        m_obstacles.push_back({ {cx, cy}, o.size });
    }

    for (const auto& p : cfg.pulseSources)
    {
        float cx = MIN_X + p.topLeft.x + p.size.x * 0.5f;
        float cy = MIN_Y + (HEIGHT - p.topLeft.y) - p.size.y * 0.5f;
        m_pulseSources.emplace_back();
        m_pulseSources.back().Initialize({ cx, cy }, p.size, 100.0f);
    }

    for (const auto& r : cfg.ramps)
    {
        float cx = MIN_X + r.topLeft.x + r.size.x * 0.5f;
        float cy = MIN_Y + (HEIGHT - r.topLeft.y) - r.size.y * 0.5f;
        m_ramps.push_back({ {cx, cy}, r.size, true });
    }
}

void Underground::Update(double dt, Player& player, Math::Vec2 playerHitboxSize)
{
    m_droneManager->Update(dt, player, playerHitboxSize, false);

    // Prep obstacle info for robot AI pathfinding/collision
    std::vector<ObstacleInfo> obstacleInfos;
    for (const auto& obs : m_obstacles) {
        obstacleInfos.push_back({ obs.pos, obs.size });
    }

    float mapMinX = MIN_X;
    float mapMaxX = MIN_X + WIDTH;

    for (auto& robot : m_robots)
    {
        robot.Update(dt, player, obstacleInfos, mapMinX, mapMaxX);
    }

    // --- Player vs Obstacle Collision Resolution (AABB) ---
    Math::Vec2 currentHitboxCenter = player.GetHitboxCenter();
    Math::Vec2 playerHalfSize = playerHitboxSize / 2.0f;

    for (const auto& obs : m_obstacles)
    {
        if (Collision::CheckAABB(currentHitboxCenter, playerHitboxSize, obs.pos, obs.size))
        {
            Math::Vec2 obsHalfSize = obs.size / 2.0f;
            Math::Vec2 obsMin = obs.pos - obsHalfSize;
            Math::Vec2 obsMax = obs.pos + obsHalfSize;

            Math::Vec2 playerMin = currentHitboxCenter - playerHalfSize;
            Math::Vec2 playerMax = currentHitboxCenter + playerHalfSize;

            // Calculate penetration depth on both axes
            float overlapX = std::min(playerMax.x, obsMax.x) - std::max(playerMin.x, obsMin.x);
            float overlapY = std::min(playerMax.y, obsMax.y) - std::max(playerMin.y, obsMin.y);

            Math::Vec2 newHitboxCenter = currentHitboxCenter;

            // Resolve along the axis with the smallest overlap
            if (overlapX < overlapY)
            {
                if (currentHitboxCenter.x < obs.pos.x)
                    newHitboxCenter.x = obsMin.x - playerHalfSize.x;
                else
                    newHitboxCenter.x = obsMax.x + playerHalfSize.x;
            }
            else
            {
                if (currentHitboxCenter.y < obs.pos.y)
                {
                    // Hit ceiling
                    newHitboxCenter.y = obsMin.y - playerHalfSize.y;
                    player.ResetVelocity();
                }
                else
                {
                    // Landed on top
                    newHitboxCenter.y = obsMax.y + playerHalfSize.y;
                    player.ResetVelocity();
                    player.SetOnGround(true);
                }
            }

            Math::Vec2 shift = newHitboxCenter - currentHitboxCenter;
            player.SetPosition(player.GetPosition() + shift);
            currentHitboxCenter = newHitboxCenter;
        }
    }

    // --- Player vs Ramp Collision Resolution ---
    float playerFootX = currentHitboxCenter.x;
    float playerFootY = currentHitboxCenter.y - playerHalfSize.y;

    for (const auto& ramp : m_ramps)
    {
        float rampHalfW = ramp.size.x / 2.0f;
        float rampHalfH = ramp.size.y / 2.0f;
        float rampLeft = ramp.pos.x - rampHalfW;
        float rampRight = ramp.pos.x + rampHalfW;
        float rampBottom = ramp.pos.y - rampHalfH;
        float rampTop = ramp.pos.y + rampHalfH;

        // Check if player is within ramp horizontal bounds
        if (playerFootX >= rampLeft && playerFootX <= rampRight &&
            playerFootY >= rampBottom && playerFootY <= rampTop + 50.0f)
        {
            // Ignore ramp if jumping upwards
            if (player.GetVelocity().y > 0.0f) continue;

            // Linear interpolation to find the height of the ramp at current X
            float localX = (playerFootX - rampLeft) / ramp.size.x;
            float targetY = rampBottom + (localX * ramp.size.y);

            // Snap player to ramp surface if close enough
            if (playerFootY <= targetY + 10.0f)
            {
                float newCenterY = targetY + playerHalfSize.y;
                Math::Vec2 shift = { 0.0f, newCenterY - currentHitboxCenter.y };
                player.SetPosition(player.GetPosition() + shift);

                player.ResetVelocity();
                player.SetOnGround(true);
                currentHitboxCenter.y = newCenterY;
            }
        }
    }
}

void Underground::Draw(Shader& shader) const
{
    // Draw background
    Math::Matrix model = Math::Matrix::CreateTranslation(m_position) * Math::Matrix::CreateScale(m_size);
    shader.setMat4("model", model);
    m_background->Draw(shader, model);

    // Draw enemies
    for (const auto& robot : m_robots)
    {
        robot.Draw(shader);
    }
    m_droneManager->Draw(shader);
}

void Underground::DrawRadars(const Shader& colorShader, DebugRenderer& debugRenderer) const
{
    m_droneManager->DrawRadars(colorShader, debugRenderer);
}

void Underground::DrawGauges(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    m_droneManager->DrawGauges(colorShader, debugRenderer);

    for (const auto& robot : m_robots)
    {
        robot.DrawGauge(colorShader, debugRenderer);
        robot.DrawAlert(colorShader, debugRenderer);
    }
}

void Underground::DrawDebug(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    // Draw collision boxes for all environment objects
    for (const auto& obs : m_obstacles)
    {
        debugRenderer.DrawBox(colorShader, obs.pos, obs.size, { 1.0f, 0.0f });
    }

    for (const auto& source : m_pulseSources)
    {
        debugRenderer.DrawBox(colorShader, source.GetPosition(), source.GetSize(), { 1.0f, 0.5f });
    }

    for (const auto& ramp : m_ramps)
    {
        debugRenderer.DrawBox(colorShader, ramp.pos, ramp.size, { 1.0f, 1.0f });
    }

}

void Underground::Shutdown()
{
    if (m_background) m_background->Shutdown();
    if (m_droneManager) m_droneManager->Shutdown();

    for (auto& robot : m_robots) robot.Shutdown();
    for (auto& source : m_pulseSources) source.Shutdown();
}

const std::vector<Drone>& Underground::GetDrones() const { return m_droneManager->GetDrones(); }
std::vector<Drone>& Underground::GetDrones() { return m_droneManager->GetDrones(); }
void Underground::ClearAllDrones() { m_droneManager->ClearAllDrones(); }
#include "Underground.hpp"
#include "Background.hpp"
#include "Player.hpp"
#include "DroneManager.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/DebugRenderer.hpp"
#include "../Engine/Collision.hpp"
#include <algorithm>
#include <cmath> 

void Underground::Initialize()
{
    m_background = std::make_unique<Background>();
    m_background->Initialize("Asset/Underground.png");

    m_size = { WIDTH, HEIGHT };
    m_position = { MIN_X + WIDTH / 2.0f, MIN_Y + HEIGHT / 2.0f };

    m_droneManager = std::make_unique<DroneManager>();

    std::vector<float> spawnXCoords = {
        18239.0f,
        21060.0f
    };

    float robotY = (MIN_Y + 90.0f) + 225.0f;

    for (float x : spawnXCoords)
    {
        m_robots.emplace_back();
        m_robots.back().Init({ x, robotY });
    }

    float droneY = MIN_Y + 650.0f;

    // 드론 속도 개별 설정
    // 1번 드론
    m_droneManager->SpawnDrone({ 19423.0f, droneY }, "Asset/Drone.png", false).SetBaseSpeed(180.0f);

    // 2번 드론
    m_droneManager->SpawnDrone({ 22203.0f, droneY }, "Asset/Drone.png", false).SetBaseSpeed(250.0f);

    // 3번 드론
    m_droneManager->SpawnDrone({ 22787.0f, droneY }, "Asset/Drone.png", false).SetBaseSpeed(400.0f);

    auto AddObstacle = [&](float topLeftX, float topLeftY, float width, float height) {
        float worldCenterX = MIN_X + topLeftX + (width / 2.0f);
        float worldCenterY = MIN_Y + (HEIGHT - topLeftY) - (height / 2.0f);
        m_obstacles.push_back({ {worldCenterX, worldCenterY}, {width, height} });
        };

    auto AddPulseSource = [&](float topLeftX, float topLeftY, float width, float height) {
        float worldCenterX = MIN_X + topLeftX + (width / 2.0f);
        float worldCenterY = MIN_Y + (HEIGHT - topLeftY) - (height / 2.0f);
        m_pulseSources.emplace_back();
        m_pulseSources.back().Initialize({ worldCenterX, worldCenterY }, { width, height }, 100.0f);
        };

    auto AddRamp = [&](float startX, float bottomY, float width, float height) {
        float centerX = startX + width / 2.0f;
        float centerY = bottomY + height / 2.0f;
        m_ramps.push_back({ {centerX, centerY}, {width, height}, true });
        };

    AddPulseSource(243.f, 480.f, 408.f, 132.f);
    AddObstacle(939.f, 834.f, 561.f, 162.f);
    AddObstacle(1584.f, 627.f, 288.f, 369.f);
    AddPulseSource(1949.f, 309.f, 69.f, 255.f);
    AddObstacle(2466.f, 834.f, 561.f, 162.f);
    AddObstacle(3471.f, 834.f, 561.f, 162.f);
    AddObstacle(4116.f, 627.f, 288.f, 369.f);
    AddPulseSource(4485.f, 309.f, 69.f, 255.f);
    AddObstacle(5235.f, 834.f, 561.f, 162.f);
    AddPulseSource(5847.f, 375.f, 1013.f, 477.f);
    AddObstacle(6825.f, 627.f, 296.f, 369.f);

    float rampStartX = 23400.0f;
    float rampBottomY = -2010.0f;
    float mapEndX = MIN_X + WIDTH;
    float rampWidth = mapEndX - rampStartX;
    float rampHeight = 300.0f;

    AddRamp(rampStartX, rampBottomY, rampWidth, rampHeight);
}

void Underground::Update(double dt, Player& player, Math::Vec2 playerHitboxSize)
{
    m_droneManager->Update(dt, player, playerHitboxSize, false);

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

            float overlapX = std::min(playerMax.x, obsMax.x) - std::max(playerMin.x, obsMin.x);
            float overlapY = std::min(playerMax.y, obsMax.y) - std::max(playerMin.y, obsMin.y);

            Math::Vec2 newHitboxCenter = currentHitboxCenter;

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
                    newHitboxCenter.y = obsMin.y - playerHalfSize.y;
                    player.ResetVelocity();
                }
                else
                {
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

        if (playerFootX >= rampLeft && playerFootX <= rampRight &&
            playerFootY >= rampBottom && playerFootY <= rampTop + 50.0f)
        {
            if (player.GetVelocity().y > 0.0f)
            {
                continue;
            }

            float localX = (playerFootX - rampLeft) / ramp.size.x;
            float targetY = rampBottom + (localX * ramp.size.y);

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
    Math::Matrix model = Math::Matrix::CreateTranslation(m_position) * Math::Matrix::CreateScale(m_size);
    shader.setMat4("model", model);
    m_background->Draw(shader, model);

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

    for (const auto& robot : m_robots)
    {
        if (!robot.IsDead())
        {
            debugRenderer.DrawBox(colorShader, robot.GetPosition(), robot.GetSize(), { 0.0f, 1.0f });
        }
    }
}

void Underground::Shutdown()
{
    if (m_background) m_background->Shutdown();
    if (m_droneManager) m_droneManager->Shutdown();

    for (auto& robot : m_robots)
    {
        robot.Shutdown();
    }

    for (auto& source : m_pulseSources)
    {
        source.Shutdown();
    }
}

const std::vector<Drone>& Underground::GetDrones() const
{
    return m_droneManager->GetDrones();
}

std::vector<Drone>& Underground::GetDrones()
{
    return m_droneManager->GetDrones();
}

void Underground::ClearAllDrones()
{
    m_droneManager->ClearAllDrones();
}
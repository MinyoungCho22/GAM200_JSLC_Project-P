#include "Underground.hpp"
#include "Background.hpp"
#include "Player.hpp"
#include "DroneManager.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/DebugRenderer.hpp"
#include "../Engine/Collision.hpp"
#include <algorithm>

void Underground::Initialize()
{
    m_background = std::make_unique<Background>();
    m_background->Initialize("Asset/Underground.png");

    m_size = { WIDTH, HEIGHT };
    m_position = { MIN_X + WIDTH / 2.0f, MIN_Y + HEIGHT / 2.0f };

    m_droneManager = std::make_unique<DroneManager>();

    auto AddObstacle = [&](float topLeftX, float topLeftY, float width, float height) {
        float worldCenterX = MIN_X + topLeftX + (width / 2.0f);
        float worldCenterY = MIN_Y + (HEIGHT - topLeftY) - (height / 2.0f);
        m_obstacles.push_back({ {worldCenterX, worldCenterY}, {width, height} });
        };

    AddObstacle(243.f, 480.f, 408.f, 132.f); //pulse
    AddObstacle(939.f, 834.f, 561.f, 162.f); //jump
    AddObstacle(1584.f, 627.f, 288.f, 369.f); //pulse
    AddObstacle(1949.f, 309.f, 69.f, 255.f); //object
    AddObstacle(2466.f, 834.f, 561.f, 162.f); //jump
    AddObstacle(3471.f, 834.f, 561.f, 162.f); //jump
    AddObstacle(4116.f, 627.f, 288.f, 369.f); //pulse
    AddObstacle(4485.f, 309.f, 69.f, 255.f); //object
    AddObstacle(5235.f, 834.f, 561.f, 162.f); //jump
    AddObstacle(5847.f, 375.f, 1013.f, 477.f); // pulse
    AddObstacle(6825.f, 627.f, 296.f, 369.f); // drone
}

void Underground::Update(double dt, Player& player, Math::Vec2 playerHitboxSize)
{
    m_droneManager->Update(dt, player, playerHitboxSize, false);

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
}

void Underground::Draw(Shader& shader) const
{
    Math::Matrix model = Math::Matrix::CreateTranslation(m_position) * Math::Matrix::CreateScale(m_size);
    shader.setMat4("model", model);
    m_background->Draw(shader, model);
    m_droneManager->Draw(shader);
}

void Underground::DrawRadars(const Shader& colorShader, DebugRenderer& debugRenderer) const
{
    m_droneManager->DrawRadars(colorShader, debugRenderer);
}

void Underground::DrawGauges(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    m_droneManager->DrawGauges(colorShader, debugRenderer);
}

void Underground::DrawDebug(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    for (const auto& obs : m_obstacles)
    {
        debugRenderer.DrawBox(colorShader, obs.pos, obs.size, { 1.0f, 0.0f });
    }
}

void Underground::Shutdown()
{
    if (m_background) m_background->Shutdown();
    if (m_droneManager) m_droneManager->Shutdown();
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
//Rooftop.cpp

#include "Rooftop.hpp"
#include "Background.hpp"
#include "Player.hpp"
#include "../Game/PulseCore.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/Collision.hpp"
#include "../Engine/Logger.hpp"
#include "../Engine/DebugRenderer.hpp"
#include <algorithm>

void Rooftop::Initialize()
{
    m_background = std::make_unique<Background>();
    m_background->Initialize("Asset/Rooftop.png");
    m_closeBackground = std::make_unique<Background>();
    m_closeBackground->Initialize("Asset/Rooftop_Close.png");

    m_size = { WIDTH, HEIGHT };
    m_position = { MIN_X + WIDTH / 2.0f, MIN_Y + HEIGHT / 2.0f };

    m_droneManager = std::make_unique<DroneManager>();

    float width = 785.f;
    float height = 172.f;
    float topLeftX = 9951.f;
    float game_Y_top = 1506.f;

    m_debugBoxPos = { topLeftX + (width / 2.0f), game_Y_top - (height / 2.0f) };
    m_debugBoxSize = { width, height };

    m_isClose = false;
    m_isPlayerClose = false;
}

void Rooftop::Update(double dt, Player& player, Math::Vec2 playerHitboxSize, Input::Input& input)
{
    Math::Vec2 playerPos = player.GetPosition();
    Math::Vec2 playerHalfSize = playerHitboxSize / 2.0f;
    Math::Vec2 playerMin = playerPos - playerHalfSize;
    Math::Vec2 playerMax = playerPos + playerHalfSize;

    Math::Vec2 boxHalfSize = m_debugBoxSize / 2.0f;
    Math::Vec2 boxMin = m_debugBoxPos - boxHalfSize;
    Math::Vec2 boxMax = m_debugBoxPos + boxHalfSize;
    Math::Vec2 closestPointOnBox;
    closestPointOnBox.x = std::clamp(playerPos.x, boxMin.x, boxMax.x);
    closestPointOnBox.y = std::clamp(playerPos.y, boxMin.y, boxMax.y);

    Math::Vec2 closestPointOnPlayer;
    closestPointOnPlayer.x = std::clamp(closestPointOnBox.x, playerMin.x, playerMax.x);
    closestPointOnPlayer.y = std::clamp(closestPointOnBox.y, playerMin.y, playerMax.y);

    float distanceSq = (closestPointOnPlayer - closestPointOnBox).LengthSq();
    const float PROXIMITY_RANGE_SQ = 100.0f * 100.0f;

    m_isPlayerClose = (distanceSq <= PROXIMITY_RANGE_SQ);

    if (m_isPlayerClose && input.IsKeyTriggered(Input::Key::F) && !m_isClose)
    {
        const float INTERACT_COST = 20.0f;
        Pulse& pulse = player.GetPulseCore().getPulse();

        if (pulse.Value() >= INTERACT_COST)
        {
            pulse.spend(INTERACT_COST);
            m_isClose = true;
        }
    }

    m_droneManager->Update(dt, player, playerHitboxSize, false);
}

void Rooftop::Draw(Shader& shader) const
{
    Math::Matrix model = Math::Matrix::CreateTranslation(m_position) * Math::Matrix::CreateScale(m_size);
    shader.setMat4("model", model);

    if (m_isClose)
    {
        m_closeBackground->Draw(shader, model);
    }
    else
    {
        m_background->Draw(shader, model);
    }

    m_droneManager->Draw(shader);
}

void Rooftop::DrawRadars(const Shader& colorShader, DebugRenderer& debugRenderer) const
{
    m_droneManager->DrawRadars(colorShader, debugRenderer);
}

void Rooftop::Shutdown()
{
    if (m_background)
    {
        m_background->Shutdown();
    }
    if (m_closeBackground)
    {
        m_closeBackground->Shutdown();
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

void Rooftop::DrawDebug(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    for (const auto& source : m_pulseSources)
    {
        debugRenderer.DrawBox(colorShader, source.GetPosition(), source.GetSize(), { 1.0f, 0.5f });
    }
    Math::Vec2 debugColor = m_isPlayerClose ? Math::Vec2(0.0f, 1.0f) : Math::Vec2(1.0f, 0.0f);
    debugRenderer.DrawBox(colorShader, m_debugBoxPos, m_debugBoxSize, debugColor);
}

const std::vector<Drone>& Rooftop::GetDrones() const
{
    return m_droneManager->GetDrones();
}

std::vector<Drone>& Rooftop::GetDrones()
{
    return m_droneManager->GetDrones();
}

void Rooftop::ClearAllDrones()
{
    m_droneManager->ClearAllDrones();
}
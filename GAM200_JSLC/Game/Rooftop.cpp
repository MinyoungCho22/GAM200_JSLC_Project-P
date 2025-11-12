// Rooftop.cpp

#include "Rooftop.hpp"
#include "Background.hpp"
#include "Player.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/Collision.hpp"
#include "../Engine/Logger.hpp"
#include "../Engine/DebugRenderer.hpp"

void Rooftop::Initialize()
{
    m_background = std::make_unique<Background>();
    m_background->Initialize("Asset/Rooftop.png");
    m_size = { WIDTH, HEIGHT };
    m_position = { MIN_X + WIDTH / 2.0f, MIN_Y + HEIGHT / 2.0f };

    m_droneManager = std::make_unique<DroneManager>();
}

void Rooftop::Update(double dt, Player& player, Math::Vec2 playerHitboxSize)
{
    m_droneManager->Update(dt, player, playerHitboxSize, false);
}

void Rooftop::Draw(Shader& shader) const
{
    Math::Matrix model = Math::Matrix::CreateTranslation(m_position) * Math::Matrix::CreateScale(m_size);
    shader.setMat4("model", model);
    m_background->Draw(shader, model);
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
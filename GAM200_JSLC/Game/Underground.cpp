#include "Underground.hpp"
#include "Background.hpp"
#include "Player.hpp"
#include "DroneManager.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/DebugRenderer.hpp"

void Underground::Initialize()
{
    m_background = std::make_unique<Background>();
    m_background->Initialize("Asset/Underground.png");

    m_size = { WIDTH, HEIGHT };
    m_position = { MIN_X + WIDTH / 2.0f, MIN_Y + HEIGHT / 2.0f };

    m_droneManager = std::make_unique<DroneManager>();
    // 필요한 경우 드론 추가: m_droneManager->SpawnDrone(...)
}

void Underground::Update(double dt, Player& player, Math::Vec2 playerHitboxSize)
{
    m_droneManager->Update(dt, player, playerHitboxSize, false);
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
    // 디버그용 경계 표시 등 필요하면 추가
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
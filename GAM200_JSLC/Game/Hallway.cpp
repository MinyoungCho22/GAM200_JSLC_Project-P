// Hallway.cpp
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

    float width = 210.f;
    float height = 312.f;
    float topLeftX = 3879.f;
    float topLeftY = 708.f;

    float bottomY = HEIGHT - topLeftY;

    Math::Vec2 center = {
        topLeftX + (width / 2.0f),
        bottomY + (height / 2.0f)
    };

    m_pulseSources.emplace_back();
    m_pulseSources.back().Initialize(center, { width, height }, 100.f);

    m_droneManager = std::make_unique<DroneManager>();
    m_droneManager->SpawnDrone({ 2500.0f, 400.0f }, "Asset/drone.png");
}

// Hallway.cpp의 Update 함수 수정
void Hallway::Update(double dt, Math::Vec2 playerCenter, Math::Vec2 playerHitboxSize, Player& player, bool isPressingE)
{
    PulseSource* closest_source = nullptr;
    float closest_dist_sq = -1.0f;

    for (auto& source : m_pulseSources)
    {
        if (!source.HasPulse()) continue;

        if (Collision::CheckAABB(playerCenter, playerHitboxSize, source.GetPosition(), source.GetSize()))
        {
            float dist_sq = (playerCenter - source.GetPosition()).LengthSq();

            if (closest_source == nullptr || dist_sq < closest_dist_sq)
            {
                closest_source = &source;
                closest_dist_sq = dist_sq;
            }
        }
    }

    bool is_near_charger = (closest_source != nullptr);

    auto result = player.GetPulseCore().tick(isPressingE, is_near_charger, false, dt);

    if (result.charged && closest_source != nullptr)
    {
        closest_source->Drain(result.delta);
    }

    m_droneManager->Update(dt, player, playerHitboxSize);
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

const std::vector<Drone>& Hallway::GetDrones() const
{
    return m_droneManager->GetDrones();
}

std::vector<Drone>& Hallway::GetDrones()
{
    return m_droneManager->GetDrones();
}

void Hallway::AttackDrone(Math::Vec2 playerPos, float attackRangeSq, Player& player)
{
    auto& drones = m_droneManager->GetDrones();
    for (auto& drone : drones)
    {
        if (!drone.IsDead())
        {
            float distSq = (playerPos - drone.GetPosition()).LengthSq();
            if (distSq < attackRangeSq)
            {
                player.GetPulseCore().getPulse().spend(20.0f);
                drone.TakeHit();
                Logger::Instance().Log(Logger::Severity::Event, "Hallway Drone has been hit!");
                break;
            }
        }
    }
}
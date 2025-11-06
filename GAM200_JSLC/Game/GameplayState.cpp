#include "GameplayState.hpp"
#include "../Engine/GameStateManager.hpp"
#include "../Engine/Engine.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Logger.hpp"
#include "../Engine/Matrix.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include "../Engine/Collision.hpp"

// ✅ 바닥 높이를 230.0f로 변경
constexpr float GROUND_LEVEL = 230.0f;
constexpr float VISUAL_Y_OFFSET = 0.0f;
constexpr float ATTACK_RANGE = 200.0f;
constexpr float ATTACK_RANGE_SQ = ATTACK_RANGE * ATTACK_RANGE;

GameplayState::GameplayState(GameStateManager& gsm_ref) : gsm(gsm_ref) {}

void GameplayState::Initialize()
{
    Logger::Instance().Log(Logger::Severity::Info, "GameplayState Initialize");
    textureShader = std::make_unique<Shader>("OpenGL/shaders/simple.vert", "OpenGL/shaders/simple.frag");
    textureShader->use();
    textureShader->setInt("ourTexture", 0);
    colorShader = std::make_unique<Shader>("OpenGL/shaders/solid_color.vert", "OpenGL/shaders/solid_color.frag");

    player.Init({ 300.0f, GROUND_LEVEL + 100.0f }, "Asset/player.png");
    pulseManager = std::make_unique<PulseManager>();

    Engine& engine = gsm.GetEngine();

    // 1. 첫 번째 펄스 공급원 (Top-left: 423, 390 / Size: 51, 63)
    float width1 = 51.f;
    float height1 = 63.f;
    float topLeftX1 = 423.f;
    float topLeftY1 = 390.f;
    Math::Vec2 center1 = { topLeftX1 + (width1 / 2.0f), topLeftY1 - (height1 / 2.0f) };
    pulseSources.emplace_back();
    pulseSources.back().Initialize(center1, { width1, height1 }, 100.f);

    // 2. 두 번째 펄스 공급원 (Top-left: 693, 525 / Size: 213, 141)
    float width2 = 213.f;
    float height2 = 141.f;
    float topLeftX2 = 693.f;
    float topLeftY2 = 525.f;
    Math::Vec2 center2 = { topLeftX2 + (width2 / 2.0f), topLeftY2 - (height2 / 2.0f) };
    pulseSources.emplace_back();
    pulseSources.back().Initialize(center2, { width2, height2 }, 100.f);

    // 3. 세 번째 펄스 공급원 (Top-left: 1413, 270 / Size: 75, 33)
    float width3 = 75.f;
    float height3 = 33.f;
    float topLeftX3 = 1413.f;
    float topLeftY3 = 270.f;
    Math::Vec2 center3 = { topLeftX3 + (width3 / 2.0f), topLeftY3 - (height3 / 2.0f) };
    pulseSources.emplace_back();
    pulseSources.back().Initialize(center3, { width3, height3 }, 100.f);

    droneManager = std::make_unique<DroneManager>();
    // 드론 생성 주석 처리
    /*
    const float roomWidth = 1620.0f;
    const float minX = (engine.GetWidth() - roomWidth) / 2.0f;
    droneManager->SpawnDrone({ minX + 620.0f, GROUND_LEVEL + 320.0f }, "Asset/Drone.png"); // 210 + 320 = 530
    droneManager->SpawnDrone({ minX + 1020.0f, GROUND_LEVEL + 120.0f }, "Asset/Drone.png"); // 210 + 120 = 330
    droneManager->SpawnDrone({ minX + 1520.0f, GROUND_LEVEL + 40.0f }, "Asset/Drone.png"); // 210 + 40 = 250
    */

    m_pulseGauge.Initialize({ 80.f, engine.GetHeight() * 0.75f }, { 40.f, 300.f });
    m_debugRenderer = std::make_unique<DebugRenderer>();
    m_debugRenderer->Initialize();

    m_room = std::make_unique<Room>();
    m_room->Initialize("Asset/Room.png");
}

void GameplayState::Update(double dt)
{
    Engine& engine = gsm.GetEngine();
    auto& input = engine.GetInput();

    if (input.IsKeyTriggered(Input::Key::Escape))
    {
        engine.RequestShutdown();
    }

    if (input.IsKeyTriggered(Input::Key::Tab))
    {
        m_isDebugDraw = !m_isDebugDraw;
    }

    Math::Vec2 playerCenter = player.GetPosition();
    Math::Vec2 playerSize = player.GetSize();
    Math::Vec2 playerHitboxSize = { playerSize.x * 0.4f, playerSize.y * 0.8f };


    if (true)
    {
        m_logTimer += dt;
        if (m_logTimer >= 0.5)
        {
            Logger::Instance().Log(Logger::Severity::Debug, "Player Pulse: %.1f / %.1f",
                player.GetPulseCore().getPulse().Value(),
                player.GetPulseCore().getPulse().Max());
            m_logTimer -= 0.5;
        }

        bool isPressingE = input.IsKeyPressed(Input::Key::E);
        pulseManager->Update(playerCenter, playerHitboxSize, player, pulseSources, isPressingE, dt);

        if (input.IsKeyPressed(Input::Key::A)) player.MoveLeft();
        if (input.IsKeyPressed(Input::Key::D)) player.MoveRight();
        if (input.IsKeyPressed(Input::Key::Space)) player.Jump();
        if (input.IsKeyPressed(Input::Key::S)) player.Crouch();
        else player.StopCrouch();
        if (input.IsKeyPressed(Input::Key::LeftShift)) player.Dash();

        if (input.IsKeyTriggered(Input::Key::F))
        {
            if (player.GetPulseCore().getPulse().Value() >= 20.0f)
            {
                auto& drones = droneManager->GetDrones();
                for (auto& drone : drones)
                {
                    float distSq = (playerCenter - drone.GetPosition()).LengthSq();
                    if (distSq < ATTACK_RANGE_SQ)
                    {
                        player.GetPulseCore().getPulse().spend(20.0f);
                        drone.TakeHit();
                        Logger::Instance().Log(Logger::Severity::Event, "Drone has been hit!");
                        break;
                    }
                }
            }
        }
    }

    droneManager->Update(dt);
    player.Update(dt);

    const auto& drones = droneManager->GetDrones();
    for (const auto& drone : drones)
    {
        Math::Vec2 droneHitboxSize = drone.GetSize() * 0.8f;

        if (Collision::CheckAABB(playerCenter, playerHitboxSize, drone.GetPosition(), droneHitboxSize))
        {
            player.TakeDamage(20.0f);
            break;
        }
    }

    const auto& pulse = player.GetPulseCore().getPulse();
    m_pulseGauge.Update(pulse.Value(), pulse.Max());

    m_room->Update(player);
}

void GameplayState::Draw()
{
    GL::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    Engine& engine = gsm.GetEngine();

    Math::Matrix projection = Math::Matrix::CreateOrtho(0.0f, static_cast<float>(engine.GetWidth()), 0.0f, static_cast<float>(engine.GetHeight()), -1.0f, 1.0f);

    m_room->Draw(engine, *textureShader, projection);

    // 펄스 공급원 그리기 (주석 처리됨)
    /*
    colorShader->use();
    colorShader->setMat4("projection", projection);
    for (const auto& source : pulseSources) {
        source.Draw(*colorShader);
    }
    */

    textureShader->use();
    textureShader->setMat4("projection", projection);
    droneManager->Draw(*textureShader);
    player.Draw(*textureShader);

    colorShader->use();
    colorShader->setMat4("projection", projection);
    m_pulseGauge.Draw(*colorShader);

    if (m_isDebugDraw)
    {
        m_room->DrawDebug(*m_debugRenderer, *colorShader, projection);

        colorShader->use();
        colorShader->setMat4("projection", projection);

        Math::Vec2 playerCenter = player.GetPosition();
        Math::Vec2 playerSize = player.GetSize();
        m_debugRenderer->DrawCircle(*colorShader, playerCenter, ATTACK_RANGE, { 1.0f, 0.0f });

        Math::Vec2 playerHitboxSize = { playerSize.x * 0.4f, playerSize.y * 0.8f };
        m_debugRenderer->DrawBox(*colorShader, playerCenter, playerHitboxSize, { 0.0f, 1.0f });

        const auto& drones = droneManager->GetDrones();
        for (const auto& drone : drones)
        {
            m_debugRenderer->DrawBox(*colorShader, drone.GetPosition(), drone.GetSize() * 0.8f, { 1.0f, 1.0f });
        }

        for (const auto& source : pulseSources)
        {
            m_debugRenderer->DrawBox(*colorShader, source.GetPosition(), source.GetSize(), { 1.0f, 0.5f });
        }
    }
}

void GameplayState::Shutdown()
{
    m_room->Shutdown();
    player.Shutdown();
    for (auto& source : pulseSources) {
        source.Shutdown();
    }
    droneManager->Shutdown();
    m_pulseGauge.Shutdown();
    m_debugRenderer->Shutdown();
    Logger::Instance().Log(Logger::Severity::Info, "GameplayState Shutdown");
}
#include "GameplayState.hpp"
#include "../Engine/GameStateManager.hpp"
#include "../Engine/Engine.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Logger.hpp"
#include "../Engine/Matrix.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include "../Engine/Collision.hpp"
#include "Setting.hpp" 
#include <GLFW/glfw3.h>
#include <string>
#include <sstream>
#include <iomanip>

// 플레이어의 물리적 바닥 높이
constexpr float GROUND_LEVEL = 230.0f;
constexpr float VISUAL_Y_OFFSET = 0.0f;
constexpr float ATTACK_RANGE = 200.0f;
constexpr float ATTACK_RANGE_SQ = ATTACK_RANGE * ATTACK_RANGE;

GameplayState::GameplayState(GameStateManager& gsm_ref) : gsm(gsm_ref) {}

void GameplayState::Initialize()
{
    Logger::Instance().Log(Logger::Severity::Info, "GameplayState Initialize");

    colorShader = std::make_unique<Shader>("OpenGL/shaders/solid_color.vert", "OpenGL/shaders/solid_color.frag");

    m_fontShader = std::make_unique<Shader>("OpenGL/shaders/simple.vert", "OpenGL/shaders/simple.frag");
    m_fontShader->use();
    m_fontShader->setInt("ourTexture", 0);

    gsm.GetEngine().GetTextureShader().use();
    gsm.GetEngine().GetTextureShader().setInt("ourTexture", 0);

    player.Init({ 300.0f, GROUND_LEVEL + 100.0f }, "Asset/player.png");
    pulseManager = std::make_unique<PulseManager>();

    Engine& engine = gsm.GetEngine();

    // --- 펄스 공급원 생성 ---
    float width1 = 51.f;
    float height1 = 63.f;
    float topLeftX1 = 410.f;
    float topLeftY1 = 390.f;
    Math::Vec2 center1 = { topLeftX1 + (width1 / 2.0f), topLeftY1 - (height1 / 2.0f) };
    pulseSources.emplace_back();
    pulseSources.back().Initialize(center1, { width1, height1 }, 100.f);

    float width2 = 208.f;
    float height2 = 141.f;
    float topLeftX2 = 673.f;
    float topLeftY2 = 525.f;
    Math::Vec2 center2 = { topLeftX2 + (width2 / 2.0f), topLeftY2 - (height2 / 2.0f) };
    pulseSources.emplace_back();
    pulseSources.back().Initialize(center2, { width2, height2 }, 100.f);

    float width3 = 75.f;
    float height3 = 33.f;
    float topLeftX3 = 1369.f;
    float topLeftY3 = 270.f;
    Math::Vec2 center3 = { topLeftX3 + (width3 / 2.0f), topLeftY3 - (height3 / 2.0f) };
    pulseSources.emplace_back();
    pulseSources.back().Initialize(center3, { width3, height3 }, 100.f);
    // --- 펄스 공급원 설정 끝 ---

    droneManager = std::make_unique<DroneManager>();

    m_pulseGauge.Initialize({ 80.f, engine.GetHeight() * 0.75f }, { 40.f, 300.f });
    m_debugRenderer = std::make_unique<DebugRenderer>();
    m_debugRenderer->Initialize();

    m_room = std::make_unique<Room>();
    m_room->Initialize(engine, "Asset/Room.png");

    m_font = std::make_unique<Font>();
    m_font->Initialize("Asset/fonts/Font_Outlined.png");

    // 정적 텍스트 및 FPS 초기 텍스트 베이킹
    m_debugToggleText = m_font->PrintToTexture(*m_fontShader, "Debug (TAB)");
    m_fpsText = m_font->PrintToTexture(*m_fontShader, "FPS: ..."); // 초기값

    // FPS 타이머 초기화 (hpp에서 이미 0.0으로 초기화됨)
    m_fpsTimer = 0.0;
    m_frameCount = 0;
}

void GameplayState::Update(double dt)
{
    Engine& engine = gsm.GetEngine();
    auto& input = engine.GetInput();

    // ESC 키 (기존 로직)
    if (input.IsKeyTriggered(Input::Key::Escape))
    {
        gsm.PushState(std::make_unique<SettingState>(gsm));
        return;
    }

    if (input.IsKeyTriggered(Input::Key::Tab))
    {
        m_isDebugDraw = !m_isDebugDraw;
    }

    // --- (기존 플레이어/게임 로직 ... ) ---
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
    // --- (기존 플레이어/게임 로직 끝) ---


    // --- 동적 텍스트 베이킹 ---

    // 1. FPS 텍스트 (1초마다 갱신)
    m_fpsTimer += dt;
    m_frameCount++;

    if (m_fpsTimer >= 1.0) // 1초가 지났다면
    {
        // FPS 텍스처를 새로 굽기
        int average_fps = static_cast<int>(m_frameCount / m_fpsTimer);
        std::stringstream ss_fps;
        ss_fps << "FPS: " << average_fps;
        m_fpsText = m_font->PrintToTexture(*m_fontShader, ss_fps.str());

        // 타이머와 카운터 리셋
        m_fpsTimer -= 1.0; // 0.0으로 리셋하는 것보다 1.0을 빼는 것이 더 정확
        m_frameCount = 0;
    }
    // (1초가 지나지 않았으면 m_fpsText는 이전 텍스처를 그대로 사용)

    // 2. 펄스 텍스트 (매 프레임 굽기)
    // (값이 동일하면 Font 캐시가 알아서 처리하므로 효율에 문제없음)
    std::stringstream ss_pulse;
    ss_pulse.precision(1); // 소수점 1자리
    ss_pulse << std::fixed << "Pulse: " << pulse.Value() << " / " << pulse.Max();
    m_pulseText = m_font->PrintToTexture(*m_fontShader, ss_pulse.str());
}

void GameplayState::Draw()
{
    Engine& engine = gsm.GetEngine();

    GL::Viewport(0, 0, engine.GetWidth(), engine.GetHeight());
    GL::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    GL::Clear(GL_COLOR_BUFFER_BIT);

    Shader& textureShader = engine.GetTextureShader();

    Math::Matrix projection = Math::Matrix::CreateOrtho(0.0f, static_cast<float>(engine.GetWidth()), 0.0f, static_cast<float>(engine.GetHeight()), -1.0f, 1.0f);

    m_room->Draw(engine, textureShader, projection);

    // ... (펄스 공급원 그리기 주석) ...

    textureShader.use();
    textureShader.setMat4("projection", projection);
    droneManager->Draw(textureShader);
    player.Draw(textureShader);

    colorShader->use();
    colorShader->setMat4("projection", projection);
    m_pulseGauge.Draw(*colorShader);

    // --- 텍스트 그리기 ---
    GL::Enable(GL_BLEND);
    GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_fontShader->use();
    m_fontShader->setMat4("projection", projection);

    // Y 좌표는 Bottom-Left (0, 0) 기준
    m_font->DrawBakedText(*m_fontShader, m_fpsText, { 20.f, engine.GetHeight() - 40.f }, 32.0f);
    m_font->DrawBakedText(*m_fontShader, m_debugToggleText, { 20.f, engine.GetHeight() - 80.f }, 32.0f);
    m_font->DrawBakedText(*m_fontShader, m_pulseText, { 20.f, engine.GetHeight() - 120.f }, 32.0f);

    // Debug Draw 로직
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
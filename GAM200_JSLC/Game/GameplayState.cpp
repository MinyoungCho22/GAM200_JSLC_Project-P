// GameplayState.cpp

#include "GameplayState.hpp"
#include "../Engine/GameStateManager.hpp"
#include "../Engine/Engine.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Logger.hpp"
#include "../Engine/Matrix.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include "../Engine/Collision.hpp"
#include "Setting.hpp"
#include "Background.hpp"
#include <GLFW/glfw3.h>
#include <string>
#include <sstream>
#include <iomanip>

constexpr float GROUND_LEVEL = 180.0f;
constexpr float VISUAL_Y_OFFSET = 0.0f;
constexpr float ATTACK_RANGE = 200.0f;
constexpr float ATTACK_RANGE_SQ = ATTACK_RANGE * ATTACK_RANGE;
constexpr float GAME_WIDTH = 1920.0f;
constexpr float GAME_HEIGHT = 1080.0f;

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

    player.Init({ 300.0f, GROUND_LEVEL + 100.0f });
    pulseManager = std::make_unique<PulseManager>();

    Engine& engine = gsm.GetEngine();

    float width1 = 51.f;
    float height1 = 63.f;
    float topLeftX1 = 424.f;
    float topLeftY1 = 360.f;
    Math::Vec2 center1 = { topLeftX1 + (width1 / 2.0f), topLeftY1 - (height1 / 2.0f) };
    pulseSources.emplace_back();
    pulseSources.back().Initialize(center1, { width1, height1 }, 100.f);

    float width2 = 215.f;
    float height2 = 180.f;
    float topLeftX2 = 692.f;
    float topLeftY2 = 550.f;
    Math::Vec2 center2 = { topLeftX2 + (width2 / 2.0f), topLeftY2 - (height2 / 2.0f) };
    pulseSources.emplace_back();
    pulseSources.back().Initialize(center2, { width2, height2 }, 100.f);

    float width3 = 75.f;
    float height3 = 33.f;
    float topLeftX3 = 1414.f;
    float topLeftY3 = 212.f;
    Math::Vec2 center3 = { topLeftX3 + (width3 / 2.0f), topLeftY3 - (height3 / 2.0f) };
    pulseSources.emplace_back();
    pulseSources.back().Initialize(center3, { width3, height3 }, 100.f);


    droneManager = std::make_unique<DroneManager>();

    m_pulseGauge.Initialize({ 80.f, GAME_HEIGHT * 0.75f }, { 40.f, 300.f });
    m_debugRenderer = std::make_unique<DebugRenderer>();
    m_debugRenderer->Initialize();

    m_room = std::make_unique<Room>();
    m_room->Initialize(engine, "Asset/Room.png");

    m_door = std::make_unique<Door>();
    m_door->Initialize({ 1710.0f, 440.0f }, { 50.0f, 300.0f }, 20.0f);

    m_hallway = std::make_unique<Hallway>();
    m_hallway->Initialize();

    // 카메라 초기화
    m_camera.Initialize({ GAME_WIDTH / 2.0f, GAME_HEIGHT / 2.0f }, GAME_WIDTH, GAME_HEIGHT);
    m_camera.SetBounds({ 0.0f, 0.0f }, { GAME_WIDTH, GAME_HEIGHT });

    m_font = std::make_unique<Font>();
    m_font->Initialize("Asset/fonts/Font_Outlined.png");

    m_debugToggleText = m_font->PrintToTexture(*m_fontShader, "Debug (TAB)");
    m_fpsText = m_font->PrintToTexture(*m_fontShader, "FPS: ...");

    m_fpsTimer = 0.0;
    m_frameCount = 0;
}

void GameplayState::Update(double dt)
{
    Engine& engine = gsm.GetEngine();
    auto& input = engine.GetInput();

    if (input.IsKeyTriggered(Input::Key::Escape))
    {
        gsm.PushState(std::make_unique<SettingState>(gsm));
        return;
    }

    if (input.IsKeyTriggered(Input::Key::Tab))
    {
        m_isDebugDraw = !m_isDebugDraw;
    }

    Math::Vec2 playerCenter = player.GetPosition();
    Math::Vec2 playerSize = player.GetSize();
    Math::Vec2 playerHitboxSize = { playerSize.x * 0.4f, playerSize.y * 0.8f + 50.0f };

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
            bool attackedDrone = false;
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
                        attackedDrone = true;
                        break;
                    }
                }
            }

            if (!attackedDrone)
            {
                m_door->Update(player, true);
            }
        }
        else
        {
            m_door->Update(player, false);
        }
    }


    // 문이 열렸는지 확인
    if (m_door->ShouldLoadNextMap() && !m_doorOpened)
    {
        Logger::Instance().Log(Logger::Severity::Event, "Door opened! Hallway accessible.");
        m_door->ResetMapTransition();
        m_doorOpened = true;

        m_room->SetRightBoundaryActive(false);
        float cameraViewWidth = GAME_WIDTH; // 1920.0f
        float roomToShow = cameraViewWidth * 0.20f; // 1920 * 0.2 = 384px // Room 보여줄만큼 설정

        // 카메라의 새로운 최소 X좌표(월드 기준)
        float newMinWorldX = GAME_WIDTH - roomToShow; // 1920 - 384 = 1536.0f

        // 카메라의 경계를 (Room의 끝자락) ~ (Hallway의 끝)으로 설정
        m_camera.SetBounds(
            { newMinWorldX, 0.0f },
            { GAME_WIDTH + Hallway::WIDTH, GAME_HEIGHT }
        );
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
    m_hallway->Update(dt);

    // 카메라 업데이트
    m_camera.Update(player.GetPosition(), 0.1f);

    m_fpsTimer += dt;
    m_frameCount++;

    if (m_fpsTimer >= 1.0)
    {
        int average_fps = static_cast<int>(m_frameCount / m_fpsTimer);
        std::stringstream ss_fps;
        ss_fps << "FPS: " << average_fps;
        m_fpsText = m_font->PrintToTexture(*m_fontShader, ss_fps.str());

        m_fpsTimer -= 1.0;
        m_frameCount = 0;
    }

    std::stringstream ss_pulse;
    ss_pulse.precision(1);
    ss_pulse << std::fixed << "Pulse: " << pulse.Value() << " / " << pulse.Max();
    m_pulseText = m_font->PrintToTexture(*m_fontShader, ss_pulse.str());
}

void GameplayState::Draw()
{
    Engine& engine = gsm.GetEngine();

    int windowWidth, windowHeight;
    glfwGetFramebufferSize(engine.GetWindow(), &windowWidth, &windowHeight);

    float windowAspect = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);
    float gameAspect = GAME_WIDTH / GAME_HEIGHT;

    int viewportX = 0;
    int viewportY = 0;
    int viewportWidth = windowWidth;
    int viewportHeight = windowHeight;

    if (windowAspect > gameAspect)
    {
        viewportWidth = static_cast<int>(windowHeight * gameAspect);
        viewportX = (windowWidth - viewportWidth) / 2;
    }
    else if (windowAspect < gameAspect)
    {
        viewportHeight = static_cast<int>(windowWidth / gameAspect);
        viewportY = (windowHeight - viewportHeight) / 2;
    }

    GL::Viewport(viewportX, viewportY, viewportWidth, viewportHeight);


    Shader& textureShader = engine.GetTextureShader();

    Math::Matrix baseProjection = Math::Matrix::CreateOrtho(
        0.0f, GAME_WIDTH,
        0.0f, GAME_HEIGHT,
        -1.0f, 1.0f
    );
    Math::Matrix view = m_camera.GetViewMatrix();
    Math::Matrix projection = baseProjection * view;

    textureShader.use();
    textureShader.setMat4("projection", projection);
    textureShader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
    textureShader.setBool("flipX", false);

    // Room 배경 그리기
    Math::Vec2 roomSize = { GAME_WIDTH, GAME_HEIGHT };
    Math::Vec2 roomCenter = { GAME_WIDTH / 2.0f, GAME_HEIGHT / 2.0f };
    Math::Matrix roomModel = Math::Matrix::CreateTranslation(roomCenter) * Math::Matrix::CreateScale(roomSize);
    textureShader.setMat4("model", roomModel);
    m_room->GetBackground()->Draw(textureShader, roomModel);

    m_hallway->Draw(textureShader); // 

    // 게임 오브젝트 그리기
    textureShader.use();
    textureShader.setMat4("projection", projection);
    droneManager->Draw(textureShader);
    player.Draw(textureShader);

    colorShader->use();
    colorShader->setMat4("projection", baseProjection);
    m_pulseGauge.Draw(*colorShader);

    GL::Enable(GL_BLEND);
    GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_fontShader->use();
    m_fontShader->setMat4("projection", baseProjection);

    m_font->DrawBakedText(*m_fontShader, m_fpsText, { 20.f, GAME_HEIGHT - 40.f }, 32.0f);
    m_font->DrawBakedText(*m_fontShader, m_debugToggleText, { 20.f, GAME_HEIGHT - 80.f }, 32.0f);
    m_font->DrawBakedText(*m_fontShader, m_pulseText, { 20.f, GAME_HEIGHT - 120.f }, 32.0f);

    if (m_isDebugDraw)
    {
        colorShader->use();
        colorShader->setMat4("projection", projection);

        Math::Vec2 playerCenter = player.GetPosition();
        Math::Vec2 playerSize = player.GetSize();
        m_debugRenderer->DrawCircle(*colorShader, playerCenter, ATTACK_RANGE, { 1.0f, 0.0f });

        Math::Vec2 playerHitboxSize = { playerSize.x * 0.4f, playerSize.y * 0.8f + 50.0f };
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

        m_door->DrawDebug(*colorShader);
    }
}

void GameplayState::Shutdown()
{
    m_room->Shutdown();
    m_hallway->Shutdown();
    player.Shutdown();
    for (auto& source : pulseSources) {
        source.Shutdown();
    }
    droneManager->Shutdown();
    m_pulseGauge.Shutdown();
    m_debugRenderer->Shutdown();
    m_door->Shutdown();

    Logger::Instance().Log(Logger::Severity::Info, "GameplayState Shutdown");
}//
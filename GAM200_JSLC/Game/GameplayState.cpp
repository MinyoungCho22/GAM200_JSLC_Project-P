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
#include <algorithm>
#include <cmath>

constexpr float GROUND_LEVEL = 180.0f;
constexpr float VISUAL_Y_OFFSET = 0.0f;
constexpr float ATTACK_RANGE = 400.0f;
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
    pulseManager->Initialize();

    m_traceSystem = std::make_unique<TraceSystem>();
    m_traceSystem->Initialize();

    Engine& engine = gsm.GetEngine();

    droneManager = std::make_unique<DroneManager>();

    m_pulseGauge.Initialize({ 75.f, GAME_HEIGHT * 0.75f - 20.0f }, { 40.f, 300.f });
    m_debugRenderer = std::make_unique<DebugRenderer>();
    m_debugRenderer->Initialize();

    m_room = std::make_unique<Room>();
    m_room->Initialize(engine, "Asset/Room.png");

    m_door = std::make_unique<Door>();
    m_door->Initialize({ 1710.0f, 440.0f }, { 50.0f, 300.0f }, 20.0f, DoorType::RoomToHallway);

    m_rooftopDoor = std::make_unique<Door>();
    m_rooftopDoor->Initialize({ 7195.0f, 400.0f }, { 300.0f, 300.0f }, 20.0f, DoorType::HallwayToRooftop);

    m_hallway = std::make_unique<Hallway>();
    m_hallway->Initialize();

    m_rooftop = std::make_unique<Rooftop>();
    m_rooftop->Initialize();

    m_camera.Initialize({ GAME_WIDTH / 2.0f, GAME_HEIGHT / 2.0f }, GAME_WIDTH, GAME_HEIGHT);
    m_camera.SetBounds({ 0.0f, 0.0f }, { GAME_WIDTH, GAME_HEIGHT });
    m_cameraSmoothSpeed = 0.1f;

    m_font = std::make_unique<Font>();
    m_font->Initialize("Asset/fonts/Font_Outlined.png");

    m_debugToggleText = m_font->PrintToTexture(*m_fontShader, "Debug (TAB)");
    m_fpsText = m_font->PrintToTexture(*m_fontShader, "FPS: ...");

    m_tutorial = std::make_unique<Tutorial>();

    auto& roomPulseSources = m_room->GetPulseSources();
    if (!roomPulseSources.empty())
    {
        m_tutorial->Init(*m_font, *m_fontShader,
            roomPulseSources[0].GetPosition(),
            roomPulseSources[0].GetSize());
    }

    for (const auto& spot : m_hallway->GetHidingSpots())
    {
        m_tutorial->AddHidingSpotMessage(*m_font, *m_fontShader, spot.pos, spot.size);
    }

    m_tutorial->AddBlindMessage(*m_font, *m_fontShader);
    m_tutorial->AddHoleMessage(*m_font, *m_fontShader);
    m_tutorial->AddRoomDoorMessage(*m_font, *m_fontShader);
    m_tutorial->AddRooftopDoorMessage(*m_font, *m_fontShader);
    m_tutorial->AddDroneCrashMessage(*m_font, *m_fontShader);
    m_tutorial->AddLiftMessage(*m_font, *m_fontShader);

    m_fpsTimer = 0.0;
    m_frameCount = 0;
    m_isGameOver = false;
}

void GameplayState::Update(double dt)
{
    Engine& engine = gsm.GetEngine();
    auto& input = engine.GetInput();

    if (m_isGameOver)
    {
        gsm.ChangeState(std::make_unique<MainMenu>(gsm));
        return;
    }

    if (input.IsKeyTriggered(Input::Key::Escape))
    {
        gsm.PushState(std::make_unique<SettingState>(gsm));
        return;
    }

    if (input.IsKeyTriggered(Input::Key::Tab))
    {
        m_isDebugDraw = !m_isDebugDraw;
    }

    m_tutorial->Update(static_cast<float>(dt), player, input, m_room.get(), m_hallway.get(), m_rooftop.get(), m_door.get(), m_rooftopDoor.get());

    Math::Vec2 playerCenter = player.GetPosition();
    Math::Vec2 playerHitboxSize = player.GetHitboxSize();
    bool isPressingE = input.IsKeyPressed(Input::Key::E);
    bool isPressingF = input.IsKeyPressed(Input::Key::F);

    const float PULSE_COST_PER_SECOND = 1.0f;

    m_logTimer += dt;
    if (m_logTimer >= 0.5)
    {
        Logger::Instance().Log(Logger::Severity::Debug, "Player Pulse: %.1f / %.1f",
            player.GetPulseCore().getPulse().Value(),
            player.GetPulseCore().getPulse().Max());
        m_logTimer -= 0.5;
    }

    pulseManager->Update(playerCenter, playerHitboxSize, player, m_room->GetPulseSources(),
        m_hallway->GetPulseSources(), m_rooftop->GetPulseSources(), isPressingE, dt);

    Drone* targetDrone = nullptr;

    if (isPressingF)
    {
        if (player.GetPulseCore().getPulse().Value() > PULSE_COST_PER_SECOND * dt)
        {
            float closestDistSq = ATTACK_RANGE_SQ;

            auto& roomDrones = droneManager->GetDrones();
            for (auto& drone : roomDrones)
            {
                if (drone.IsDead() || drone.IsHit()) continue;
                float distSq = (playerCenter - drone.GetPosition()).LengthSq();
                Math::Vec2 droneHitboxSize = drone.GetSize() * 0.8f;
                float droneHitboxRadius = (droneHitboxSize.x + droneHitboxSize.y) * 0.25f;
                float effectiveAttackRange = ATTACK_RANGE + droneHitboxRadius;
                float effectiveAttackRangeSq = effectiveAttackRange * effectiveAttackRange;
                if (distSq < effectiveAttackRangeSq && distSq < closestDistSq)
                {
                    closestDistSq = distSq;
                    targetDrone = &drone;
                }
            }

            auto& hallwayDrones = m_hallway->GetDrones();
            for (auto& drone : hallwayDrones)
            {
                if (drone.IsDead() || drone.IsHit()) continue;
                float distSq = (playerCenter - drone.GetPosition()).LengthSq();
                Math::Vec2 droneHitboxSize = drone.GetSize() * 0.8f;
                float droneHitboxRadius = (droneHitboxSize.x + droneHitboxSize.y) * 0.25f;
                float effectiveAttackRange = ATTACK_RANGE + droneHitboxRadius;
                float effectiveAttackRangeSq = effectiveAttackRange * effectiveAttackRange;
                if (distSq < effectiveAttackRangeSq && distSq < closestDistSq)
                {
                    closestDistSq = distSq;
                    targetDrone = &drone;
                }
            }

            auto& rooftopDrones = m_rooftop->GetDrones();
            for (auto& drone : rooftopDrones)
            {
                if (drone.IsDead() || drone.IsHit()) continue;
                float distSq = (playerCenter - drone.GetPosition()).LengthSq();
                Math::Vec2 droneHitboxSize = drone.GetSize() * 0.8f;
                float droneHitboxRadius = (droneHitboxSize.x + droneHitboxSize.y) * 0.25f;
                float effectiveAttackRange = ATTACK_RANGE + droneHitboxRadius;
                float effectiveAttackRangeSq = effectiveAttackRange * effectiveAttackRange;
                if (distSq < effectiveAttackRangeSq && distSq < closestDistSq)
                {
                    closestDistSq = distSq;
                    targetDrone = &drone;
                }
            }
        }
    }

    if (isPressingF && targetDrone != nullptr)
    {
        player.GetPulseCore().getPulse().spend(PULSE_COST_PER_SECOND * static_cast<float>(dt));

        bool didDroneDie = targetDrone->ApplyDamage(static_cast<float>(dt));
        if (didDroneDie)
        {
            m_traceSystem->OnDroneKilled(*droneManager);
        }

        Math::Vec2 vfxStartPos = { playerCenter.x + (player.IsFacingRight() ? 1 : -1) * (playerHitboxSize.x / 2.0f), playerCenter.y };
        pulseManager->UpdateAttackVFX(true, vfxStartPos, targetDrone->GetPosition());

        m_door->Update(player, false);
        m_rooftopDoor->Update(player, false);
    }
    else
    {
        pulseManager->UpdateAttackVFX(false, {}, {});
        for (auto& drone : droneManager->GetDrones()) drone.ResetDamageTimer();
        for (auto& drone : m_hallway->GetDrones()) drone.ResetDamageTimer();
        for (auto& drone : m_rooftop->GetDrones()) drone.ResetDamageTimer();

        if (input.IsKeyTriggered(Input::Key::F))
        {
            m_door->Update(player, true);
            m_rooftopDoor->Update(player, true);
        }
        else
        {
            m_door->Update(player, false);
            m_rooftopDoor->Update(player, false);
        }
    }

    if (m_door->ShouldLoadNextMap() && !m_doorOpened)
    {
        HandleRoomToHallwayTransition();
    }

    if (m_rooftopDoor->ShouldLoadNextMap() && !m_rooftopAccessed)
    {
        HandleHallwayToRooftopTransition();
    }

    bool isPlayerHidingInRoom = m_room->IsPlayerHiding(playerCenter, playerHitboxSize, player.IsCrouching());
    bool isPlayerHidingInHallway = m_hallway->IsPlayerHiding(playerCenter, playerHitboxSize, player.IsCrouching());
    bool isPlayerHiding = isPlayerHidingInRoom || isPlayerHidingInHallway;

    droneManager->Update(dt, player, playerHitboxSize, isPlayerHiding);
    player.Update(dt, input);

    auto& drones = droneManager->GetDrones();
    for (auto& drone : drones)
    {
        if (!drone.IsDead() && drone.ShouldDealDamage())
        {
            player.TakeDamage(10.0f);
            drone.ResetDamageFlag();
            break;
        }
    }

    const auto& pulse = player.GetPulseCore().getPulse();
    m_pulseGauge.Update(pulse.Value(), pulse.Max());

    if (!m_rooftopAccessed)
    {
        m_room->Update(player, dt, input);
    }

    m_hallway->Update(dt, playerCenter, playerHitboxSize, player, isPlayerHiding);
    m_rooftop->Update(dt, player, playerHitboxSize, input);

    auto& hallwayDrones = m_hallway->GetDrones();
    for (auto& drone : hallwayDrones)
    {
        if (!drone.IsDead() && drone.ShouldDealDamage())
        {
            player.TakeDamage(10.0f);
            drone.ResetDamageFlag();
            break;
        }
    }

    auto& rooftopDrones = m_rooftop->GetDrones();
    for (auto& drone : rooftopDrones)
    {
        if (!drone.IsDead() && drone.ShouldDealDamage())
        {
            player.TakeDamage(10.0f);
            drone.ResetDamageFlag();
            break;
        }
    }

    m_camera.Update(player.GetPosition(), m_cameraSmoothSpeed);

    static double cameraLogTimer = 0.0f;
    cameraLogTimer += dt;
    if (cameraLogTimer > 1.0f && m_rooftopAccessed)
    {
        Math::Vec2 camPos = m_camera.GetPosition();
        Logger::Instance().Log(Logger::Severity::Debug,
            "Camera: (%.1f, %.1f), Player: (%.1f, %.1f)",
            camPos.x, camPos.y, playerCenter.x, playerCenter.y);
        cameraLogTimer = 0.0f;
    }

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

    if (player.IsDead())
    {
        gsm.PushState(std::make_unique<GameOver>(gsm, m_isGameOver));
    }
}

void GameplayState::HandleRoomToHallwayTransition()
{
    Logger::Instance().Log(Logger::Severity::Event, "Door opened! Hallway accessible.");
    m_door->ResetMapTransition();
    m_doorOpened = true;

    m_room->SetRightBoundaryActive(false);
    float cameraViewWidth = GAME_WIDTH;
    float roomToShow = cameraViewWidth * 0.20f;
    float newMinWorldX = GAME_WIDTH - roomToShow;

    float worldMaxX = GAME_WIDTH + Hallway::WIDTH;
    float worldMaxY = GAME_HEIGHT;

    m_camera.SetBounds(
        { newMinWorldX, 0.0f },
        { worldMaxX, worldMaxY }
    );

    m_cameraSmoothSpeed = 0.1f;
}

void GameplayState::HandleHallwayToRooftopTransition()
{
    m_rooftopDoor->ResetMapTransition();
    m_rooftopAccessed = true;

    float newGroundLevel = Rooftop::MIN_Y + GROUND_LEVEL + 200.0f;
    player.SetCurrentGroundLevel(newGroundLevel);

    float playerStartX = Rooftop::MIN_X + 1550.0f;
    float playerStartY = Rooftop::MIN_Y + (Rooftop::HEIGHT / 2.0f + 200.0f);

    player.SetPosition({ playerStartX, playerStartY });
    player.ResetVelocity();
    player.SetOnGround(false);

    float worldMinX = Rooftop::MIN_X;
    float worldMaxX = Rooftop::MIN_X + Rooftop::WIDTH;
    float worldMinY = Rooftop::MIN_Y;
    float worldMaxY = Rooftop::MIN_Y + Rooftop::HEIGHT;

    m_camera.SetBounds({ worldMinX, worldMinY }, { worldMaxX, worldMaxY });
    m_cameraSmoothSpeed = 0.02f;

    Logger::Instance().Log(Logger::Severity::Event,
        "Rooftop: Player=(%.1f, %.1f), Ground=%.1f",
        playerStartX, playerStartY, newGroundLevel);
}

void GameplayState::Draw()
{
    Engine& engine = gsm.GetEngine();

    int windowWidth, windowHeight;
    glfwGetFramebufferSize(engine.GetWindow(), &windowWidth, &windowHeight);

    float r, g, b;
    Math::Vec2 playerPos = player.GetPosition();

    if (playerPos.y >= Rooftop::MIN_Y)
    {
        r = 70.0f / 255.0f; g = 68.0f / 255.0f; b = 71.0f / 255.0f;
    }
    else
    {
        if (playerPos.x < GAME_WIDTH)
        {
            r = 12.0f / 255.0f; g = 12.0f / 255.0f; b = 12.0f / 255.0f;
        }
        else
        {
            r = 70.0f / 255.0f; g = 68.0f / 255.0f; b = 71.0f / 255.0f;
        }
    }

    GL::Viewport(0, 0, windowWidth, windowHeight);
    GL::ClearColor(r, g, b, 1.0f);
    GL::Clear(GL_COLOR_BUFFER_BIT);


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

    m_room->Draw(textureShader);
    m_hallway->Draw(textureShader);
    m_rooftop->Draw(textureShader);

    textureShader.use();
    textureShader.setMat4("projection", projection);
    droneManager->Draw(textureShader);
    player.Draw(textureShader);

    textureShader.use();
    textureShader.setMat4("projection", projection);
    pulseManager->DrawVFX(textureShader);

    colorShader->use();
    colorShader->setMat4("projection", projection);
    droneManager->DrawRadars(*colorShader, *m_debugRenderer);
    m_hallway->DrawRadars(*colorShader, *m_debugRenderer);
    m_rooftop->DrawRadars(*colorShader, *m_debugRenderer);

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

    std::string countdownText = m_rooftop->GetLiftCountdownText();
    if (!countdownText.empty())
    {
        CachedTextureInfo countdownTexture = m_font->PrintToTexture(*m_fontShader, countdownText);
        m_font->DrawBakedText(*m_fontShader, countdownTexture, { GAME_WIDTH / 2.0f - 250.0f, 100.0f }, 50.0f);
    }

    m_tutorial->Draw(*m_font, *m_fontShader);

    if (m_isDebugDraw)
    {
        colorShader->use();
        colorShader->setMat4("projection", projection);

        Math::Vec2 playerCenter = player.GetHitboxCenter();
        m_debugRenderer->DrawCircle(*colorShader, playerCenter, ATTACK_RANGE, { 1.0f, 0.0f });

        Math::Vec2 playerHitboxSize = player.GetHitboxSize();
        m_debugRenderer->DrawBox(*colorShader, playerCenter, playerHitboxSize, { 0.0f, 1.0f });

        const auto& drones = droneManager->GetDrones();
        for (const auto& drone : drones)
        {
            if (!drone.IsDead())
            {
                m_debugRenderer->DrawBox(*colorShader, drone.GetPosition(), drone.GetSize() * 0.8f, { 1.0f, 1.0f });
            }
        }

        for (const auto& drone : m_hallway->GetDrones())
        {
            if (!drone.IsDead())
            {
                m_debugRenderer->DrawBox(*colorShader, drone.GetPosition(), drone.GetSize() * 0.8f, { 1.0f, 1.0f });
                m_debugRenderer->DrawCircle(*colorShader, drone.GetPosition(), Drone::DETECTION_RANGE, { 1.0f, 0.5f });
            }
        }

        for (const auto& drone : m_rooftop->GetDrones())
        {
            if (!drone.IsDead())
            {
                m_debugRenderer->DrawBox(*colorShader, drone.GetPosition(), drone.GetSize() * 0.8f, { 1.0f, 1.0f });
                m_debugRenderer->DrawCircle(*colorShader, drone.GetPosition(), Drone::DETECTION_RANGE, { 1.0f, 0.5f });
            }
        }

        m_room->DrawDebug(*m_debugRenderer, *colorShader, projection, player);

        for (const auto& source : m_hallway->GetPulseSources())
        {
            m_debugRenderer->DrawBox(*colorShader, source.GetPosition(), source.GetSize(), { 1.0f, 0.5f });
        }

        m_hallway->DrawDebug(*colorShader, *m_debugRenderer);
        m_rooftop->DrawDebug(*colorShader, *m_debugRenderer);
        m_door->DrawDebug(*colorShader);
        m_rooftopDoor->DrawDebug(*colorShader);
    }
}

void GameplayState::Shutdown()
{
    m_room->Shutdown();
    m_hallway->Shutdown();
    m_rooftop->Shutdown();
    player.Shutdown();
    droneManager->Shutdown();
    m_pulseGauge.Shutdown();
    m_debugRenderer->Shutdown();
    m_door->Shutdown();
    m_rooftopDoor->Shutdown();
    pulseManager->Shutdown();

    Logger::Instance().Log(Logger::Severity::Info, "GameplayState Shutdown");
}
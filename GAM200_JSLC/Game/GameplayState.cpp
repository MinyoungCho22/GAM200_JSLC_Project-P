//GameplayState.cpp

#include "GameplayState.hpp"
#include "../Engine/GameStateManager.hpp"
#include "../Engine/Engine.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Logger.hpp"
#include "../Engine/Matrix.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include "../Engine/Collision.hpp"
#include "../Engine/ImguiManager.hpp"
#include "Setting.hpp"
#include "GameOver.hpp"
#include "MapObjectConfig.hpp"
#include "Background.hpp"
#include <GLFW/glfw3.h>
#include <string>
#include <sstream>

#include "../Engine/Sound.hpp"

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

    colorShader = std::make_unique<Shader>("OpenGL/Shaders/solid_color.vert", "OpenGL/Shaders/solid_color.frag");
    colorShader->use();
    colorShader->setFloat("uAlpha", 1.0f);

    m_fontShader = std::make_unique<Shader>("OpenGL/Shaders/simple.vert", "OpenGL/Shaders/simple.frag");
    m_fontShader->use();
    m_fontShader->setInt("ourTexture", 0);

    m_outlineShader = std::make_unique<Shader>("OpenGL/Shaders/simple.vert", "OpenGL/Shaders/outline.frag");
    m_outlineShader->use();
    m_outlineShader->setInt("ourTexture", 0);

    gsm.GetEngine().GetTextureShader().use();
    gsm.GetEngine().GetTextureShader().setInt("ourTexture", 0);

    player.Init({ 200.0f, GROUND_LEVEL + 100.0f });

    pulseManager = std::make_unique<PulseManager>();
    pulseManager->Initialize();

    m_traceSystem = std::make_unique<TraceSystem>();
    m_traceSystem->Initialize();

    Engine& engine = gsm.GetEngine();

    droneManager = std::make_unique<DroneManager>();

    m_pulseGauge.Initialize({ 75.f, GAME_HEIGHT * 0.75f - 70.0f }, { 40.f, 300.f });
    m_debugRenderer = std::make_unique<DebugRenderer>();
    m_debugRenderer->Initialize();

    // Load map object config before map initialization.
    MapObjectConfig::Instance().Load();

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

    m_underground = std::make_unique<Underground>();
    m_underground->Initialize();
    m_undergroundAccessed = false;

    m_subway = std::make_unique<Subway>();
    m_subway->Initialize();
    m_subwayAccessed = false;

    // Apply JSON config once at startup (the same path used by hot-reload).
    {
        const auto& cfg = MapObjectConfig::Instance().GetData();
        m_room->ApplyConfig(cfg.room);
        m_hallway->ApplyConfig(cfg.hallway);
        m_rooftop->ApplyConfig(cfg.rooftop);
        m_underground->ApplyConfig(cfg.underground);
        m_subway->ApplyConfig(cfg.subway);
    }

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
    m_doorOpened = false;

    if (m_bgm.Load("Asset/BackgroundMusic.mp3", true))
    {
        m_bgm.Play();
        m_bgm.SetVolume(0.4f);
        Logger::Instance().Log(Logger::Severity::Info, "Gameplay BGM Started");
    }

    // Set up debug window with drone managers and config manager
    auto* imguiManager = gsm.GetEngine().GetImguiManager();
    if (imguiManager)
    {
        // Add map drone managers
        imguiManager->AddMapDroneManager("Hallway", m_hallway->GetDroneManager());
        imguiManager->AddMapDroneManager("Rooftop", m_rooftop->GetDroneManager());
        imguiManager->AddMapDroneManager("Underground", m_underground->GetDroneManager());
        imguiManager->AddMapDroneManager("Subway", m_subway->GetDroneManager());

        // Set main drone manager (for backwards compatibility)
        if (droneManager)
        {
            imguiManager->SetDroneManager(droneManager.get());
            imguiManager->AddMapDroneManager("Main", droneManager.get());
        }

        // Set underground for robot debugging
        imguiManager->SetUnderground(m_underground.get());

        // Set config manager
        auto configManager = gsm.GetEngine().GetDroneConfigManager();
        if (configManager)
        {
            imguiManager->SetDroneConfigManager(configManager);

            configManager->LoadLiveStatesFromFile();

            for (size_t i = 0; i < droneManager->GetDrones().size(); ++i)
            {
                configManager->ApplyLiveStateToDrone("Main", static_cast<int>(i), const_cast<Drone&>(droneManager->GetDrones()[i]));
            }

            auto& hallwayDrones = m_hallway->GetDrones();
            for (size_t i = 0; i < hallwayDrones.size(); ++i)
            {
                configManager->ApplyLiveStateToDrone("Hallway", static_cast<int>(i), hallwayDrones[i]);
            }

            auto& rooftopDrones = m_rooftop->GetDrones();
            for (size_t i = 0; i < rooftopDrones.size(); ++i)
            {
                configManager->ApplyLiveStateToDrone("Rooftop", static_cast<int>(i), rooftopDrones[i]);
            }

            auto& undergroundDrones = m_underground->GetDrones();
            for (size_t i = 0; i < undergroundDrones.size(); ++i)
            {
                configManager->ApplyLiveStateToDrone("Underground", static_cast<int>(i), undergroundDrones[i]);
            }

            auto& subwayDrones = m_subway->GetDrones();
            for (size_t i = 0; i < subwayDrones.size(); ++i)
            {
                configManager->ApplyLiveStateToDrone("Subway", static_cast<int>(i), subwayDrones[i]);
            }
        }

        // Set robot config manager
        auto robotConfigManager = gsm.GetEngine().GetRobotConfigManager();
        if (robotConfigManager)
        {
            imguiManager->SetRobotConfigManager(robotConfigManager);

            robotConfigManager->LoadLiveStatesFromFile();

            auto& robots = m_underground->GetRobots();
            for (size_t i = 0; i < robots.size(); ++i)
            {
                robotConfigManager->ApplyLiveStateToRobot("Underground", static_cast<int>(i), robots[i]);
            }
        }
    }
}

void GameplayState::Update(double dt)
{
    Engine& engine = gsm.GetEngine();
    auto& input = engine.GetInput();

    // Update player god mode from ImGui settings
    auto* imguiManager = engine.GetImguiManager();
    if (imguiManager)
    {
        player.SetGodMode(imguiManager->IsPlayerGodMode());
    }

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

    if (input.IsKeyPressed(Input::Key::LeftControl) && input.IsKeyTriggered(Input::Key::Num2))
    {
        if (!m_doorOpened)
        {
            m_tutorial->DisableAll();
            HandleRoomToHallwayTransition();

            player.SetPosition({ 2000.0f, GROUND_LEVEL + 100.0f });
            player.ResetVelocity();

            Logger::Instance().Log(Logger::Severity::Event, "Cheat: Teleported to Hallway");
        }
    }

    if (input.IsKeyPressed(Input::Key::LeftControl) && input.IsKeyTriggered(Input::Key::Num3))
    {
        if (!m_rooftopAccessed)
        {
            m_tutorial->DisableAll();
            HandleHallwayToRooftopTransition();
        }
    }

    if (input.IsKeyPressed(Input::Key::LeftControl) && input.IsKeyTriggered(Input::Key::Num4))
    {
        if (!m_undergroundAccessed)
        {
            m_tutorial->DisableAll();
            HandleRooftopToUndergroundTransition();
        }
    }

    if (input.IsKeyPressed(Input::Key::LeftControl) && input.IsKeyTriggered(Input::Key::Num5))
    {
        if (!m_subwayAccessed)
        {
            m_tutorial->DisableAll();
            HandleUndergroundToSubwayTransition();
        }
    }

    m_tutorial->Update(static_cast<float>(dt), player, input, m_room.get(), m_hallway.get(), m_rooftop.get(), m_door.get(), m_rooftopDoor.get());

    Math::Vec2 playerCenter = player.GetPosition();
    Math::Vec2 playerHitboxSize = player.GetHitboxSize();

    bool isPressingInteract = input.IsMouseButtonPressed(Input::MouseButton::Right);
    bool isPressingAttack = input.IsMouseButtonPressed(Input::MouseButton::Left);

    // Get mouse world position
    double mouseScreenX, mouseScreenY;
    input.GetMousePosition(mouseScreenX, mouseScreenY);
    Math::Vec2 mouseWorldPos = ScreenToWorldCoordinates(mouseScreenX, mouseScreenY);

    // Auto hot-reload on file save for map object coordinates/sizes/sprites.
    if (MapObjectConfig::Instance().ReloadIfChanged())
    {
        const auto& cfg = MapObjectConfig::Instance().GetData();
        m_room->ApplyConfig(cfg.room);
        m_hallway->ApplyConfig(cfg.hallway);
        m_rooftop->ApplyConfig(cfg.rooftop);
        m_underground->ApplyConfig(cfg.underground);
        m_subway->ApplyConfig(cfg.subway);
    }

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
        m_hallway->GetPulseSources(), m_rooftop->GetPulseSources(), m_underground->GetPulseSources(),
        m_subway->GetPulseSources(), isPressingInteract, dt, mouseWorldPos);

    Drone* targetDrone = nullptr;
    Robot* targetRobot = nullptr;

    if (isPressingAttack)
    {
        // Check for god mode (infinite pulse)
        auto* imguiManager = gsm.GetEngine().GetImguiManager();
        bool isGodMode = imguiManager && imguiManager->IsPlayerGodMode();

        if (isGodMode || player.GetPulseCore().getPulse().Value() > PULSE_COST_PER_SECOND * dt)
        {
            float closestDistSq = ATTACK_RANGE_SQ;

            auto checkDrones = [&](std::vector<Drone>& drones) {
                const Math::Vec2 cursorHitbox = { 32.0f, 32.0f };
                for (auto& drone : drones)
                {
                    if (drone.IsDead() || drone.IsHit()) continue;
                    float distSq = (playerCenter - drone.GetPosition()).LengthSq();
                    Math::Vec2 droneHitboxSize = drone.GetSize() * 0.8f;
                    float droneHitboxRadius = (droneHitboxSize.x + droneHitboxSize.y) * 0.25f;
                    float effectiveAttackRange = ATTACK_RANGE + droneHitboxRadius;
                    float effectiveAttackRangeSq = effectiveAttackRange * effectiveAttackRange;

                    bool isMouseOnDrone = Collision::CheckPointInAABB(mouseWorldPos, drone.GetPosition(), droneHitboxSize)
                        || Collision::CheckAABB(mouseWorldPos, cursorHitbox, drone.GetPosition(), droneHitboxSize);

                    if (distSq < effectiveAttackRangeSq && distSq < closestDistSq && isMouseOnDrone)
                    {
                        closestDistSq = distSq;
                        targetDrone = &drone;
                        targetRobot = nullptr;
                    }
                }
            };

            checkDrones(droneManager->GetDrones());
            checkDrones(m_hallway->GetDrones());
            checkDrones(m_rooftop->GetDrones());
            if (m_undergroundAccessed) checkDrones(m_underground->GetDrones());
            if (m_subwayAccessed) checkDrones(m_subway->GetDrones());

            if (m_undergroundAccessed)
            {
                auto& robots = m_underground->GetRobots();
                const Math::Vec2 cursorHitbox = { 32.0f, 32.0f };

                for (auto& robot : robots)
                {
                    if (robot.IsDead()) continue;

                    float distSq = (playerCenter - robot.GetPosition()).LengthSq();
                    Math::Vec2 robotSize = robot.GetSize();
                    float robotRadius = (robotSize.x + robotSize.y) * 0.25f;
                    float effectiveAttackRange = ATTACK_RANGE + robotRadius;
                    float effectiveAttackRangeSq = effectiveAttackRange * effectiveAttackRange;

                    bool isMouseOnRobot = Collision::CheckPointInAABB(mouseWorldPos, robot.GetPosition(), robotSize)
                        || Collision::CheckAABB(mouseWorldPos, cursorHitbox, robot.GetPosition(), robotSize);

                    if (distSq < effectiveAttackRangeSq && distSq < closestDistSq && isMouseOnRobot)
                    {
                        closestDistSq = distSq;
                        targetRobot = &robot;
                        targetDrone = nullptr;
                    }
                }
            }

            if (m_subwayAccessed)
            {
                auto& robots = m_subway->GetRobots();
                const Math::Vec2 cursorHitbox = { 32.0f, 32.0f };

                for (auto& robot : robots)
                {
                    if (robot.IsDead()) continue;

                    float distSq = (playerCenter - robot.GetPosition()).LengthSq();
                    Math::Vec2 robotSize = robot.GetSize();
                    float robotRadius = (robotSize.x + robotSize.y) * 0.25f;
                    float effectiveAttackRange = ATTACK_RANGE + robotRadius;
                    float effectiveAttackRangeSq = effectiveAttackRange * effectiveAttackRange;

                    bool isMouseOnRobot = Collision::CheckPointInAABB(mouseWorldPos, robot.GetPosition(), robotSize)
                        || Collision::CheckAABB(mouseWorldPos, cursorHitbox, robot.GetPosition(), robotSize);

                    if (distSq < effectiveAttackRangeSq && distSq < closestDistSq && isMouseOnRobot)
                    {
                        closestDistSq = distSq;
                        targetRobot = &robot;
                        targetDrone = nullptr;
                    }
                }
            }
        }
    }

    if (isPressingAttack && (targetDrone != nullptr || targetRobot != nullptr))
    {
        auto* imguiManager = gsm.GetEngine().GetImguiManager();
        bool isGodMode = imguiManager && imguiManager->IsPlayerGodMode();

        if (!isGodMode)
        {
            player.GetPulseCore().getPulse().spend(PULSE_COST_PER_SECOND * static_cast<float>(dt));
        }

        Math::Vec2 targetPos;

        if (targetDrone != nullptr)
        {
            // Cache position before potential reinforcement spawn.
            // OnDroneKilled may spawn drones and reallocate vectors, invalidating targetDrone pointer.
            targetPos = targetDrone->GetPosition();
            bool didDroneDie = targetDrone->ApplyDamage(static_cast<float>(dt));
            if (didDroneDie)
            {
                m_traceSystem->OnDroneKilled(*droneManager, player.GetPosition());
            }
        }
        else if (targetRobot != nullptr)
        {
            float damagePerSecond = 30.0f;

            if (Collision::CheckAABB(playerCenter, playerHitboxSize, targetRobot->GetPosition(), targetRobot->GetSize()))
            {
                damagePerSecond = 50.0f;
            }

            targetRobot->TakeDamage(damagePerSecond * static_cast<float>(dt));
            targetPos = targetRobot->GetPosition();
        }

        Math::Vec2 vfxStartPos = { playerCenter.x + (player.IsFacingRight() ? 1 : -1) * (playerHitboxSize.x / 2.0f), playerCenter.y };
        pulseManager->UpdateAttackVFX(true, vfxStartPos, targetPos);

        m_door->Update(player, false, mouseWorldPos);
        m_rooftopDoor->Update(player, false, mouseWorldPos);
    }
    else
    {
        pulseManager->UpdateAttackVFX(false, {}, {});

        if (input.IsMouseButtonTriggered(Input::MouseButton::Left))
        {
            if (m_room->IsBlindOpen())
            {
                m_door->Update(player, true, mouseWorldPos);
            }
            else
            {
                m_door->Update(player, false, mouseWorldPos);
            }

            m_rooftopDoor->Update(player, true, mouseWorldPos);
        }
        else
        {
            m_door->Update(player, false, mouseWorldPos);
            m_rooftopDoor->Update(player, false, mouseWorldPos);
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

    if (m_rooftopAccessed && !m_undergroundAccessed)
    {
        float playerRight = player.GetPosition().x + player.GetHitboxSize().x * 0.5f;
        float transitionX = Rooftop::MIN_X + Rooftop::WIDTH - 1.0f;
        if (playerRight >= transitionX)
        {
            HandleRooftopToUndergroundTransition();
        }
    }

    if (m_undergroundAccessed && !m_subwayAccessed)
    {
        float transitionX = Underground::MIN_X + Underground::WIDTH - 50.0f;
        if (player.GetPosition().x > transitionX)
        {
            HandleUndergroundToSubwayTransition();
        }
    }

    bool isPlayerHidingInRoom = m_room->IsPlayerHiding(playerCenter, playerHitboxSize, player.IsCrouching());
    bool isPlayerHidingInHallway = m_hallway->IsPlayerHiding(playerCenter, playerHitboxSize, player.IsCrouching());
    bool isPlayerHiding = isPlayerHidingInRoom || isPlayerHidingInHallway;

    player.SetHiding(isPlayerHiding);

    droneManager->Update(dt, player, playerHitboxSize, isPlayerHiding);
    player.Update(dt, input);

    if (m_doorOpened)
    {
        float leftLimit = GAME_WIDTH + player.GetHitboxSize().x * 0.5f + 235.0f;

        if (player.GetPosition().x < leftLimit)
        {
            player.SetPosition({ leftLimit, player.GetPosition().y });

            Math::Vec2 currentVel = player.GetVelocity();
            if (currentVel.x < 0.0f)
            {
                player.ResetVelocity();
            }
        }
    }

    auto& drones = droneManager->GetDrones();
    for (auto& drone : drones)
    {
        if (!drone.IsDead() && drone.ShouldDealDamage())
        {
            auto* imguiManager = gsm.GetEngine().GetImguiManager();
            if (!imguiManager || !imguiManager->IsPlayerGodMode())
            {
                player.TakeDamage(10.0f);
            }
            drone.ResetDamageFlag();
            break;
        }
    }

    const auto& pulse = player.GetPulseCore().getPulse();
    m_pulseGauge.Update(pulse.Value(), pulse.Max());

    if (!m_rooftopAccessed)
    {
        m_room->Update(player, dt, input, mouseWorldPos);
    }

    auto& pp = engine.GetPostProcess();

    if (m_doorOpened && !m_rooftopAccessed)
        pp.Settings().exposure = 0.35f;
    else
        pp.Settings().exposure = 1.0f;

    m_hallway->Update(dt, playerCenter, playerHitboxSize, player, isPlayerHiding);
    m_rooftop->Update(dt, player, playerHitboxSize, input, mouseWorldPos,
                      input.IsMouseButtonTriggered(Input::MouseButton::Left));

    if (m_undergroundAccessed)
    {
        m_underground->Update(dt, player, playerHitboxSize);
    }

    if (m_subwayAccessed)
    {
        m_subway->Update(dt, player, playerHitboxSize);
    }

    auto& hallwayDrones = m_hallway->GetDrones();
    for (auto& drone : hallwayDrones)
    {
        if (!drone.IsDead() && drone.ShouldDealDamage())
        {
            auto* imguiManager = gsm.GetEngine().GetImguiManager();
            if (!imguiManager || !imguiManager->IsPlayerGodMode())
            {
                player.TakeDamage(10.0f);
            }
            drone.ResetDamageFlag();
            break;
        }
    }

    auto& rooftopDrones = m_rooftop->GetDrones();
    for (auto& drone : rooftopDrones)
    {
        if (!drone.IsDead() && drone.ShouldDealDamage())
        {
            auto* imguiManager = gsm.GetEngine().GetImguiManager();
            if (!imguiManager || !imguiManager->IsPlayerGodMode())
            {
                player.TakeDamage(10.0f);
            }
            drone.ResetDamageFlag();
            break;
        }
    }

    if (m_undergroundAccessed)
    {
        auto& undergroundDrones = m_underground->GetDrones();
        for (auto& drone : undergroundDrones)
        {
            if (!drone.IsDead() && drone.ShouldDealDamage())
            {
                auto* imguiManager = gsm.GetEngine().GetImguiManager();
                if (!imguiManager || !imguiManager->IsPlayerGodMode())
                {
                    player.TakeDamage(20.0f);
                }
                drone.ResetDamageFlag();
                break;
            }
        }
    }

    if (m_subwayAccessed)
    {
        auto& subwayDrones = m_subway->GetDrones();
        for (auto& drone : subwayDrones)
        {
            if (!drone.IsDead() && drone.ShouldDealDamage())
            {
                auto* imguiManager = gsm.GetEngine().GetImguiManager();
                if (!imguiManager || !imguiManager->IsPlayerGodMode())
                {
                    player.TakeDamage(25.0f);
                }
                drone.ResetDamageFlag();
                break;
            }
        }
    }

    // Update camera animation
    if (m_camera.IsAnimating())
    {
        m_camera.UpdateAnimation(static_cast<float>(dt));
    }
    else
    {
        m_camera.Update(player.GetPosition(), m_cameraSmoothSpeed);
    }

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

    std::stringstream ss_warning;
    ss_warning << "Warning Level: " << m_traceSystem->GetWarningLevel();
    m_warningLevelText = m_font->PrintToTexture(*m_fontShader, ss_warning.str());

    if (engine.GetImguiManager())
    {
        engine.GetImguiManager()->SetWarningLevel(m_traceSystem->GetWarningLevel());
    }

    if (player.IsDead())
    {
        // Ensure hall exposure does not leak into GameOver rendering.
        engine.GetPostProcess().Settings().exposure = 1.0f;
        gsm.PushState(std::make_unique<GameOver>(gsm, m_isGameOver));
        return;
    }

    SoundSystem::Instance().Update();
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

void GameplayState::HandleRooftopToUndergroundTransition()
{
    m_rooftopAccessed = true;
    m_undergroundAccessed = true;

    m_rooftop->ClearAllDrones();

    float playerStartX = Underground::MIN_X + 100.0f;
    float playerStartY = Underground::MIN_Y + 300.0f;

    float newGroundLevel = Underground::MIN_Y + 90.0f;
    player.SetCurrentGroundLevel(newGroundLevel);

    player.SetPosition({ playerStartX, playerStartY });
    player.ResetVelocity();
    player.SetOnGround(false);

    float worldMinX = Underground::MIN_X;
    float worldMaxX = Underground::MIN_X + Underground::WIDTH;
    float worldMinY = Underground::MIN_Y;
    float worldMaxY = Underground::MIN_Y + Underground::HEIGHT;

    m_camera.SetBounds({ worldMinX, worldMinY }, { worldMaxX, worldMaxY });

    m_cameraSmoothSpeed = 0.05f;

    Logger::Instance().Log(Logger::Severity::Event,
        "Transition to Underground! Player=(%.1f, %.1f)", playerStartX, playerStartY);
}

void GameplayState::HandleUndergroundToSubwayTransition()
{
    Logger::Instance().Log(Logger::Severity::Event,
        "Transition to Subway! Starting descent animation...");

    m_undergroundAccessed = true;
    m_subwayAccessed = true;

    m_underground->ClearAllDrones();

    float playerStartX = Subway::MIN_X + 300.0f;
    float playerStartY = Subway::MIN_Y + 540.0f;
    float newGroundLevel = Subway::MIN_Y + 90.0f;

    player.SetCurrentGroundLevel(newGroundLevel);
    player.SetPosition({ playerStartX, playerStartY });
    player.ResetVelocity();
    player.SetOnGround(false);

    float worldMinX = Subway::MIN_X;
    float worldMaxX = Subway::MIN_X + Subway::WIDTH;
    float worldMinY = Subway::MIN_Y;
    float worldMaxY = Subway::MIN_Y + Subway::HEIGHT;

    m_camera.SetBounds({ worldMinX, worldMinY }, { worldMaxX, worldMaxY });

    Math::Vec2 cameraStartPos = m_camera.GetPosition();
    Math::Vec2 cameraEndPos = { Subway::MIN_X + GAME_WIDTH / 2.0f, Subway::MIN_Y + GAME_HEIGHT / 2.0f };

    m_camera.StartAnimation(cameraStartPos, cameraEndPos, 2.0f);

    m_cameraSmoothSpeed = 0.05f;

    Logger::Instance().Log(Logger::Severity::Event,
        "Subway Transition! Camera: (%.1f, %.1f) -> (%.1f, %.1f), Player: (%.1f, %.1f)",
        cameraStartPos.x, cameraStartPos.y, cameraEndPos.x, cameraEndPos.y,
        playerStartX, playerStartY);
}

Math::Vec2 GameplayState::ScreenToWorldCoordinates(double screenX, double screenY) const
{
    Engine& engine = gsm.GetEngine();
    int windowWidth, windowHeight;
    glfwGetWindowSize(engine.GetWindow(), &windowWidth, &windowHeight);

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

    float mouseViewportX = static_cast<float>(screenX - viewportX);
    float mouseViewportY = static_cast<float>(screenY - viewportY);

    float mouseNDCX = mouseViewportX / static_cast<float>(viewportWidth);
    float mouseNDCY = mouseViewportY / static_cast<float>(viewportHeight);

    float mouseGameX = mouseNDCX * GAME_WIDTH;
    float mouseGameY = (1.0f - mouseNDCY) * GAME_HEIGHT;

    Math::Vec2 cameraPos = m_camera.GetPosition();
    Math::Vec2 worldPos;
    worldPos.x = mouseGameX - (GAME_WIDTH / 2.0f) + cameraPos.x;
    worldPos.y = mouseGameY - (GAME_HEIGHT / 2.0f) + cameraPos.y;

    return worldPos;
}

void GameplayState::DrawMainLayer()
{
    // If GameOver has just popped, do not render the previous gameplay frame.
    if (m_isGameOver)
    {
        GL::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        GL::Clear(GL_COLOR_BUFFER_BIT);
        return;
    }

    Engine& engine = gsm.GetEngine();

    float r, g, b;
    Math::Vec2 playerPos = player.GetPosition();

    if (playerPos.y >= Rooftop::MIN_Y)
    {
        r = 70.0f / 255.0f; g = 68.0f / 255.0f; b = 71.0f / 255.0f;
    }
    else if (playerPos.y <= Subway::MIN_Y + Subway::HEIGHT)
    {
        r = 15.0f / 255.0f; g = 15.0f / 255.0f; b = 20.0f / 255.0f;
    }
    else if (playerPos.y <= Underground::MIN_Y + Underground::HEIGHT)
    {
        r = 30.0f / 255.0f; g = 30.0f / 255.0f; b = 35.0f / 255.0f;
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

    // Viewport is already set to FBO size (GAME_WIDTH x GAME_HEIGHT) by PostProcessManager::BeginScene().
    GL::ClearColor(r, g, b, 1.0f);
    GL::Clear(GL_COLOR_BUFFER_BIT);

    GL::Enable(GL_BLEND);
    GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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
    m_underground->Draw(textureShader);
    m_subway->Draw(textureShader);

    textureShader.use();
    textureShader.setMat4("projection", projection);
    droneManager->Draw(textureShader);
    player.Draw(textureShader);

    m_hallway->DrawForeground(textureShader);

    textureShader.use();
    textureShader.setMat4("projection", projection);
    pulseManager->DrawVFX(textureShader);
    GL::Disable(GL_BLEND);
}

void GameplayState::DrawForegroundLayer()
{
    if (m_isGameOver)
        return;

    Engine& engine = gsm.GetEngine();

    Math::Matrix baseProjection = Math::Matrix::CreateOrtho(
        0.0f, GAME_WIDTH,
        0.0f, GAME_HEIGHT,
        -1.0f, 1.0f
    );
    Math::Matrix view = m_camera.GetViewMatrix();
    Math::Matrix projection = baseProjection * view;

    // Always draw foreground stuff after post-process.
    GL::Enable(GL_BLEND);
    GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // 1) Outlines
    m_outlineShader->use();
    m_outlineShader->setMat4("projection", projection);

    {
        Math::Vec2 playerPos = player.GetPosition();
        m_hallway->DrawSpriteOutlines(*m_outlineShader, playerPos);
        m_rooftop->DrawSpriteOutlines(*m_outlineShader, playerPos);
    }

    if (m_isDebugDraw)
    {
        player.DrawOutline(*m_outlineShader);

        for (const auto& robot : m_underground->GetRobots())
        {
            if (!robot.IsDead())
                robot.DrawOutline(*m_outlineShader);
        }

        for (const auto& robot : m_subway->GetRobots())
        {
            if (!robot.IsDead())
                robot.DrawOutline(*m_outlineShader);
        }
    }

    // 2) World-space overlays (radars / gauges)
    colorShader->use();
    colorShader->setMat4("projection", projection);

    droneManager->DrawRadars(*colorShader, *m_debugRenderer);
    m_hallway->DrawRadars(*colorShader, *m_debugRenderer);
    m_rooftop->DrawRadars(*colorShader, *m_debugRenderer);
    m_underground->DrawRadars(*colorShader, *m_debugRenderer);
    m_subway->DrawRadars(*colorShader, *m_debugRenderer);

    droneManager->DrawGauges(*colorShader, *m_debugRenderer);
    m_hallway->DrawGauges(*colorShader, *m_debugRenderer);
    m_rooftop->DrawGauges(*colorShader, *m_debugRenderer);
    m_underground->DrawGauges(*colorShader, *m_debugRenderer);
    m_subway->DrawGauges(*colorShader, *m_debugRenderer);

    // 3) HUD
    colorShader->use();
    colorShader->setMat4("projection", baseProjection);
    colorShader->setFloat("uAlpha", 1.0f);
    GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_pulseGauge.Draw(*colorShader);

    // 4) Fonts / tutorial
    m_fontShader->use();
    m_fontShader->setMat4("projection", baseProjection);

    m_font->DrawBakedText(*m_fontShader, m_fpsText, { 20.f, GAME_HEIGHT - 40.f }, 32.0f);
    m_font->DrawBakedText(*m_fontShader, m_debugToggleText, { 20.f, GAME_HEIGHT - 80.f }, 32.0f);
    m_font->DrawBakedText(*m_fontShader, m_pulseText, { 20.f, GAME_HEIGHT - 120.f }, 32.0f);
    m_font->DrawBakedText(*m_fontShader, m_warningLevelText, { 20.f, GAME_HEIGHT - 160.f }, 32.0f);

    std::string countdownText = m_rooftop->GetLiftCountdownText();
    if (!countdownText.empty())
    {
        CachedTextureInfo countdownTexture = m_font->PrintToTexture(*m_fontShader, countdownText);
        m_font->DrawBakedText(*m_fontShader, countdownTexture, { GAME_WIDTH / 2.0f - 250.0f, 100.0f }, 50.0f);
    }

    m_tutorial->Draw(*m_font, *m_fontShader);

    // 5) Cursor
    Engine& engineForCursor = gsm.GetEngine();
    auto& inputForCursor = engineForCursor.GetInput();

    double mouseScreenX, mouseScreenY;
    inputForCursor.GetMousePosition(mouseScreenX, mouseScreenY);

    int windowWidthForCursor, windowHeightForCursor;
    glfwGetWindowSize(engineForCursor.GetWindow(), &windowWidthForCursor, &windowHeightForCursor);

    float windowAspectForCursor = static_cast<float>(windowWidthForCursor) / static_cast<float>(windowHeightForCursor);
    float gameAspectForCursor = GAME_WIDTH / GAME_HEIGHT;

    int viewportXForCursor = 0;
    int viewportYForCursor = 0;
    int viewportWidthForCursor = windowWidthForCursor;
    int viewportHeightForCursor = windowHeightForCursor;

    if (windowAspectForCursor > gameAspectForCursor)
    {
        viewportWidthForCursor = static_cast<int>(windowHeightForCursor * gameAspectForCursor);
        viewportXForCursor = (windowWidthForCursor - viewportWidthForCursor) / 2;
    }
    else if (windowAspectForCursor < gameAspectForCursor)
    {
        viewportHeightForCursor = static_cast<int>(windowWidthForCursor / gameAspectForCursor);
        viewportYForCursor = (windowHeightForCursor - viewportHeightForCursor) / 2;
    }

    float mouseViewportX = static_cast<float>(mouseScreenX - viewportXForCursor);
    float mouseViewportY = static_cast<float>(mouseScreenY - viewportYForCursor);

    float mouseNDCX = mouseViewportX / static_cast<float>(viewportWidthForCursor);
    float mouseNDCY = mouseViewportY / static_cast<float>(viewportHeightForCursor);

    float mouseGameX = mouseNDCX * GAME_WIDTH;
    float mouseGameY = (1.0f - mouseNDCY) * GAME_HEIGHT;

    colorShader->use();
    colorShader->setMat4("projection", baseProjection);

    const float cursorSize = 16.0f;
    const float cursorThick = 2.5f;
    Math::Vec2 cursorPos = { mouseGameX, mouseGameY };

    // Additive blending: cursor adds light to the scene, always visible in darkness
    GL::BlendFunc(GL_SRC_ALPHA, GL_ONE);

    // Outer glow
    colorShader->setFloat("uAlpha", 0.07f);
    m_debugRenderer->DrawBox(*colorShader, cursorPos, { cursorSize * 3.2f, cursorThick * 6.0f }, { 0.0f, 1.0f });
    m_debugRenderer->DrawBox(*colorShader, cursorPos, { cursorThick * 6.0f, cursorSize * 3.2f }, { 0.0f, 1.0f });
    m_debugRenderer->DrawCircle(*colorShader, cursorPos, 24.0f, { 0.0f, 1.0f });

    // Mid glow
    colorShader->setFloat("uAlpha", 0.18f);
    m_debugRenderer->DrawBox(*colorShader, cursorPos, { cursorSize * 2.5f, cursorThick * 3.5f }, { 0.0f, 1.0f });
    m_debugRenderer->DrawBox(*colorShader, cursorPos, { cursorThick * 3.5f, cursorSize * 2.5f }, { 0.0f, 1.0f });
    m_debugRenderer->DrawCircle(*colorShader, cursorPos, 17.0f, { 0.0f, 1.0f });

    // Bright core
    colorShader->setFloat("uAlpha", 0.85f);
    m_debugRenderer->DrawBox(*colorShader, cursorPos, { cursorSize * 1.8f, cursorThick }, { 0.0f, 1.0f });
    m_debugRenderer->DrawBox(*colorShader, cursorPos, { cursorThick, cursorSize * 1.8f }, { 0.0f, 1.0f });
    m_debugRenderer->DrawCircle(*colorShader, cursorPos, 5.5f, { 0.0f, 1.0f });

    // Restore default alpha blend for any later overlay/debug draws
    GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    colorShader->setFloat("uAlpha", 1.0f);

    // 6) Debug overlay
    if (m_isDebugDraw)
    {
        colorShader->use();
        colorShader->setMat4("projection", projection);

        Math::Vec2 playerCenter = player.GetHitboxCenter();
        m_debugRenderer->DrawCircle(*colorShader, playerCenter, ATTACK_RANGE, { 1.0f, 0.0f });

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

        for (const auto& drone : m_underground->GetDrones())
        {
            if (!drone.IsDead())
            {
                m_debugRenderer->DrawBox(*colorShader, drone.GetPosition(), drone.GetSize() * 0.8f, { 1.0f, 1.0f });
                m_debugRenderer->DrawCircle(*colorShader, drone.GetPosition(), Drone::DETECTION_RANGE, { 1.0f, 0.5f });
            }
        }

        for (const auto& drone : m_subway->GetDrones())
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
            // Show fallback debug box only when no sprite exists.
            if (!source.HasSprite())
            {
                m_debugRenderer->DrawBox(*colorShader, source.GetPosition(), source.GetSize(), { 1.0f, 0.5f });
            }
        }

        m_hallway->DrawDebug(*colorShader, *m_debugRenderer);
        m_rooftop->DrawDebug(*colorShader, *m_debugRenderer);
        m_underground->DrawDebug(*colorShader, *m_debugRenderer);
        m_subway->DrawDebug(*colorShader, *m_debugRenderer);
        m_door->DrawDebug(*colorShader);
        m_rooftopDoor->DrawDebug(*colorShader);
    }

    GL::Disable(GL_BLEND);
}

void GameplayState::Shutdown()
{
    if (auto* imguiManager = gsm.GetEngine().GetImguiManager())
    {
        imguiManager->ClearGameplayBindings();
    }

    m_room->Shutdown();
    m_hallway->Shutdown();
    m_rooftop->Shutdown();
    m_underground->Shutdown();
    m_subway->Shutdown();
    player.Shutdown();
    droneManager->Shutdown();
    m_pulseGauge.Shutdown();
    m_debugRenderer->Shutdown();
    m_door->Shutdown();
    m_rooftopDoor->Shutdown();
    pulseManager->Shutdown();

    m_bgm.Stop();

    Logger::Instance().Log(Logger::Severity::Info, "GameplayState Shutdown");
}
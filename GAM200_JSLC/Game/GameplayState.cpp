//GameplayState.cpp

#include "GameplayState.hpp"
#include "../Engine/GameStateManager.hpp"
#include "../Engine/ControlBindings.hpp"
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
#include <string>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <utility>
#include <vector>

#include "../Engine/Sound.hpp"

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

constexpr float GROUND_LEVEL = 180.0f;
constexpr float VISUAL_Y_OFFSET = 0.0f;
constexpr float ATTACK_RANGE = 400.0f;
constexpr float ATTACK_RANGE_SQ = ATTACK_RANGE * ATTACK_RANGE;
constexpr float GAME_WIDTH = 1920.0f;
constexpr float GAME_HEIGHT = 1080.0f;
constexpr float ROOM_PLAYABLE_WIDTH = 1620.0f;
constexpr float ROOM_LEFT_BOUNDARY = (GAME_WIDTH - ROOM_PLAYABLE_WIDTH) * 0.5f;

// Gamepad: wider drone “aim” box than mouse (collision unchanged elsewhere).
constexpr float kGamepadDroneAimHitboxScale = 2.35f;
// Magnet snap in virtual view pixels (~game projection units).
constexpr float kPadMagnetInnerGame = 42.0f;
constexpr float kPadMagnetOuterGame = 125.0f;
constexpr float kPadManualSnapMaxWorld = 3000.0f;

constexpr float PLAYER_INSET = 20.0f;
constexpr float DRONE_TARGET_INSET = 16.0f;
constexpr float ROBOT_CORE_INSET_X = 100.0f;
constexpr float ROBOT_CORE_OFFSET_Y = 34.0f;

constexpr float TRAIN_PLAYER_INSET = 30.0f;
constexpr float TRAIN_DRONE_TARGET_INSET = 8.0f;
constexpr float TRAIN_ROBOT_CORE_INSET_X = 80.0f;
constexpr float TRAIN_ROBOT_CORE_OFFSET_Y = 18.0f;

namespace
{
    Math::Vec2 DronePulseTargetCenter(const Drone& d)
    {
        return d.GetPosition();
    }
} // namespace

// Hallway hiding-box S.png: only while hall post-process is on; fades by player distance to spot top.
constexpr float HIDING_S_PROMPT_DISTANCE = 300.0f;
constexpr float HIDING_S_PROMPT_DISTANCE_SQ = HIDING_S_PROMPT_DISTANCE * HIDING_S_PROMPT_DISTANCE;
constexpr float HIDING_S_ICON_WORLD_SIZE = 80.0f;
constexpr float HIDING_S_ICON_OFFSET_Y = 55.0f;

// Hallway entry spawn: slightly right/down from the previous defaults (door + Ctrl+2).
constexpr float HALLWAY_ENTRY_MARGIN_X = 235.0f + 65.0f;
constexpr float HALLWAY_FLOOR_VERTICAL_ADJUST = -20.0f;
constexpr float HALLWAY_GROUND_LEVEL = GROUND_LEVEL + HALLWAY_FLOOR_VERTICAL_ADJUST;
constexpr float HALLWAY_ENTRY_POS_Y = GROUND_LEVEL + 60.0f + 50.0f + HALLWAY_FLOOR_VERTICAL_ADJUST;

constexpr float OPENING_STORY_DELAY_SEC = 1.5f;
constexpr float HALLWAY_ENTRY_STORY_DELAY_SEC = 1.0f;
/// Caps dt so a single huge tick (first frame, focus loss) does not skip story delays.
constexpr float STORY_DELAY_DT_CAP = 0.1f;

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

    player.Init({ ROOM_LEFT_BOUNDARY, GROUND_LEVEL + 100.0f });
    // Start at the same position Room::Update would clamp to, so the player
    // does not slide right during the initial fade-in.
    player.SetPosition({
        ROOM_LEFT_BOUNDARY + player.GetSize().x * 0.5f,
        GROUND_LEVEL + player.GetSize().y * 0.5f
    });
    player.SetOnGround(true);

    pulseManager = std::make_unique<PulseManager>();
    pulseManager->Initialize();

    m_traceSystem = std::make_unique<TraceSystem>();
    m_traceSystem->Initialize();

    Engine& engine = gsm.GetEngine();

    droneManager = std::make_unique<DroneManager>();

    m_pulseGauge.Initialize();
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

    m_train = std::make_unique<Train>();
    m_train->Initialize();
    m_trainAccessed = false;
    m_trainDeferEntryUntilIntroDone = false;
    m_rooftopAccessed = false;
    m_pulseDetonateSkill.Initialize();

    // Apply JSON config once at startup (the same path used by hot-reload).
    {
        const auto& cfg = MapObjectConfig::Instance().GetData();
        m_room->ApplyConfig(cfg.room);
        m_hallway->ApplyConfig(cfg.hallway);
        m_rooftop->ApplyConfig(cfg.rooftop);
        m_underground->ApplyConfig(cfg.underground);
        m_train->ApplyConfig(cfg.train);
    }

    m_camera.Initialize({ GAME_WIDTH / 2.0f, GAME_HEIGHT / 2.0f }, GAME_WIDTH, GAME_HEIGHT);
    m_camera.SetBounds({ 0.0f, 0.0f }, { GAME_WIDTH, GAME_HEIGHT });
    m_cameraSmoothSpeed = 0.1f;

    m_font = std::make_unique<Font>();
    m_font->Initialize("Asset/fonts/Font_Outlined.png");

    m_fpsText = m_font->PrintToTexture(*m_fontShader, "FPS: ...");

    // Custom cursor sprites
    m_mouseIdleCursor = std::make_unique<Background>();
    m_mouseIdleCursor->InitializeWithBlackKeyTransparency("Asset/MouseIdle.png");
    m_mousePointerCursor = std::make_unique<Background>();
    m_mousePointerCursor->InitializeWithBlackKeyTransparency("Asset/MousePointer.png");
    m_mouseLeftCursor = std::make_unique<Background>();
    m_mouseLeftCursor->InitializeWithBlackKeyTransparency("Asset/MouseLeft.png");
    m_mouseRightCursor = std::make_unique<Background>();
    m_mouseRightCursor->InitializeWithBlackKeyTransparency("Asset/MouseRight.png");

    m_hudFrame = std::make_unique<Background>();
    m_hudFrame->Initialize("Asset/Hud.png");

    m_conversionBackdrop = std::make_unique<Background>();
    m_conversionBackdrop->Initialize("Asset/Conversion.png");

    m_hallwayHidingPromptS = std::make_unique<Background>();
    m_hallwayHidingPromptS->Initialize("Asset/S.png");

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

    m_storyDialogue = std::make_unique<StoryDialogue>();
    m_storyDialogue->Initialize();
    m_openingStoryDelayRemaining = OPENING_STORY_DELAY_SEC;
    m_wasBlindOpen = false;
    m_roomBlindLowPulseStoryDone = false;
    m_wasNearLiftRooftop = false;
    m_rooftopLiftStoryDone = false;
    m_hallwayFaradayBoxStoryDone = false;
    m_hallwayEntryStoryPending = false;
    m_hallwayEntryStoryDelayRemaining = 0.0f;

    m_fpsTimer = 0.0;
    m_frameCount = 0;
    m_isGameOver = false;
    m_blockAmbientStoryForSession = false;
    m_doorOpened = false;
    m_currentCheckpoint = MapZone::Room;
    m_pendingTransition = PendingTransition::None;
    m_prevRooftopForQHint = false;
    m_skipRooftopQHintByCheat = false;
    m_rooftopQStoryDone = false;
    m_rooftopQStoryPending = false;
    m_rooftopQStoryDelayRemaining = 0.0f;
    m_fadeAlpha = 1.0f;                // start fully black
    m_fadeState = FadeState::FadingIn; // fade in from black on game start

    // Scene must be dark from the very first Draw (before Update sets exposure).
    gsm.GetEngine().GetPostProcess().Settings().exposure = 0.0f;
    // Snap camera to the bounded position so it doesn't lerp during fade-in.
    m_camera.Update(player.GetPosition(), 1.0f);

    // Fade overlay: centered unit quad (-0.5..0.5)
    if (m_fadeVAO == 0)
    {
        float fadeVerts[] = {
            -0.5f,  0.5f,   0.5f,  0.5f,  -0.5f, -0.5f,
             0.5f,  0.5f,   0.5f, -0.5f,  -0.5f, -0.5f
        };
        GL::GenVertexArrays(1, &m_fadeVAO);
        GL::GenBuffers(1, &m_fadeVBO);
        GL::BindVertexArray(m_fadeVAO);
        GL::BindBuffer(GL_ARRAY_BUFFER, m_fadeVBO);
        GL::BufferData(GL_ARRAY_BUFFER, sizeof(fadeVerts), fadeVerts, GL_STATIC_DRAW);
        GL::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        GL::EnableVertexAttribArray(0);
        GL::BindVertexArray(0);
    }

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
        imguiManager->AddMapDroneManager("Train", m_train->GetDroneManager());

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
            m_underground->ReapplyEntryTracerDroneAfterLiveState();

            auto& trainDrones = m_train->GetDrones();
            for (size_t i = 0; i < trainDrones.size(); ++i)
            {
                configManager->ApplyLiveStateToDrone("Train", static_cast<int>(i), trainDrones[i]);
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
    auto& ctl = engine.GetControlBindings();

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
        gsm.PushState(std::make_unique<SettingState>(gsm, true));
        return;
    }

    // Train zoom-out + player shrink transition (runs every frame, including during fade)
    if (m_trainZoomTransition)
    {
        const float zoomFdt = std::min(static_cast<float>(dt), 1.0f / 30.0f);
        m_trainZoomTimer += zoomFdt;
        float t = std::min(m_trainZoomTimer / TRAIN_ZOOM_DURATION, 1.0f);
        // Ease-in-out
        float easedT = t < 0.5f
            ? 2.0f * t * t
            : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
        m_cameraZoom = TRAIN_ZOOM_START + (1.0f - TRAIN_ZOOM_START) * easedT;
        player.SetSizeScale(1.0f + (0.6f - 1.0f) * easedT);
        if (m_trainZoomTimer >= TRAIN_ZOOM_DURATION)
        {
            m_trainZoomTransition = false;
            m_cameraZoom = 1.0f;
            player.SetSizeScale(0.6f);
        }
    }

    // Map transition fade update: freeze player input but keep camera alive
    if (m_fadeState != FadeState::None)
    {
        // Cap fdt so a large dt spike (e.g. from blocking Initialize()) doesn't
        // consume the entire fade-in in a single frame and make it look like a pop.
        const float fdt = std::min(static_cast<float>(dt), 1.0f / 30.0f);

        // Apply hallway post-process during Room→Hallway fade so the shader
        // is visible before the fade-in, not after it.
        {
            bool isHallwayFade = (m_pendingTransition == PendingTransition::RoomToHallway)
                              || (m_doorOpened && !m_rooftopAccessed);
            if (isHallwayFade)
            {
                auto& pp = gsm.GetEngine().GetPostProcess();
                pp.Settings().exposure             = 0.4f;
                pp.Settings().useLightOverlay      = true;
                pp.Settings().lightOverlayStrength = 0.6f;
                pp.Settings().cameraPos            = m_camera.GetPosition();
            }
        }

        if (m_fadeState == FadeState::FadingOut)
        {
            m_fadeAlpha = std::min(1.0f, m_fadeAlpha + fdt / FADE_OUT_DURATION);
            if (m_fadeAlpha >= 1.0f)
                ExecutePendingTransition(); // teleports player, resets camera pos
        }
        else if (m_fadeState == FadeState::FadingIn)
        {
            m_fadeAlpha = std::max(0.0f, m_fadeAlpha - fdt / FADE_IN_DURATION);
            if (m_fadeAlpha <= 0.0f)
            {
                m_fadeAlpha = 0.0f;
                m_fadeState = FadeState::None;
            }

            // Belt-and-suspenders: also drive PostProcess exposure so the scene
            // itself darkens inside the FBO, not just the foreground overlay.
            // Only for game-start fade (no map transition pending, room only).
            // Skip when hallway PP (useLightOverlay) is active to avoid conflict.
            {
                auto& pp = gsm.GetEngine().GetPostProcess();
                if (!pp.Settings().useLightOverlay)
                    pp.Settings().exposure = 1.0f - m_fadeAlpha;
            }
        }
        // Keep camera smoothly tracking even while screen is fading
        if (m_camera.IsAnimating())
            m_camera.UpdateAnimation(fdt);
        else
            m_camera.Update(player.GetPosition(), m_cameraSmoothSpeed);
        SoundSystem::Instance().Update();
        return;
    }

    bool suppressAmbientStoryThisFrame = false;

    if (m_trainDeferEntryUntilIntroDone)
    {
        if (!m_trainAccessed || !m_train)
            m_trainDeferEntryUntilIntroDone = false;
        else if (!m_trainZoomTransition)
        {
            m_train->StartEntryTimer();
            m_trainDeferEntryUntilIntroDone = false;
        }
    }

    const auto silenceStoryForMapCheat = [&]() {
        m_blockAmbientStoryForSession = true;
        suppressAmbientStoryThisFrame = true;
        if (m_storyDialogue)
            m_storyDialogue->ResetForNewRun();
    };

    if (input.IsKeyPressed(Input::Key::LeftControl) && input.IsKeyTriggered(Input::Key::Num1))
    {
        silenceStoryForMapCheat();
        m_tutorial->DisableAll();
        m_doorOpened = false;
        m_rooftopAccessed = false;
        m_undergroundAccessed = false;
        m_trainAccessed = false;
        m_trainDeferEntryUntilIntroDone = false;
        m_room->SetRightBoundaryActive(true);
        m_door->ResetMapTransition();
        m_rooftopDoor->ResetMapTransition();
        m_camera.StopAnimation();
        m_camera.SetBounds({ 0.0f, 0.0f }, { GAME_WIDTH, GAME_HEIGHT });
        m_cameraSmoothSpeed = 0.1f;
        m_cameraZoom = 1.0f;
        m_trainZoomTransition = false;
        player.SetSizeScale(1.0f);
        player.SetCurrentGroundLevel(GROUND_LEVEL);
        player.SetPosition({
            ROOM_LEFT_BOUNDARY + player.GetSize().x * 0.5f,
            GROUND_LEVEL + player.GetSize().y * 0.5f
        });
        player.ResetVelocity();
        player.SetOnGround(true);
        m_camera.Update(player.GetPosition(), 1.0f);
        engine.GetPostProcess().Settings().exposure = 1.0f;
        engine.GetPostProcess().Settings().useLightOverlay = false;
        engine.GetPostProcess().Settings().lightOverlayStrength = 0.0f;
        m_wasBlindOpen = false;
        m_roomBlindLowPulseStoryDone = false;
        m_wasNearLiftRooftop = false;
        m_rooftopLiftStoryDone = false;
        m_hallwayFaradayBoxStoryDone = false;
        m_hallwayEntryStoryPending = false;
        m_hallwayEntryStoryDelayRemaining = 0.0f;
        m_rooftopQStoryPending = false;
        m_rooftopQStoryDelayRemaining = 0.0f;
        m_openingStoryDelayRemaining = 0.0f;
        m_currentCheckpoint = MapZone::Room;
        Logger::Instance().Log(Logger::Severity::Event, "Cheat: Teleported to Room (Ctrl+1)");
    }

    if (input.IsKeyPressed(Input::Key::LeftControl) && input.IsKeyTriggered(Input::Key::Num2))
    {
        silenceStoryForMapCheat();
        m_hallwayEntryStoryPending = false;
        m_hallwayEntryStoryDelayRemaining = 0.0f;
        m_cameraZoom = 1.0f;
        m_trainZoomTransition = false;
        player.SetSizeScale(1.0f);
        m_currentCheckpoint = MapZone::Hallway;

        const float hallwaySpawnX = GAME_WIDTH + player.GetHitboxSize().x * 0.5f + HALLWAY_ENTRY_MARGIN_X;
        const float hallwaySpawnY = HALLWAY_GROUND_LEVEL + player.GetSize().y * 0.5f;

        if (m_rooftopAccessed && !m_undergroundAccessed)
        {
            m_tutorial->DisableAll();
            OpenHallwayDoorLayoutOnly();
            m_rooftopAccessed = false;
            m_rooftopDoor->ResetMapTransition();
            m_camera.StopAnimation();
            player.SetCurrentGroundLevel(HALLWAY_GROUND_LEVEL);
            player.SetPosition({ hallwaySpawnX, hallwaySpawnY });
            player.ResetVelocity();
            player.SetOnGround(true);
            m_camera.SetPosition(player.GetPosition());
            Logger::Instance().Log(Logger::Severity::Event, "Cheat: Rooftop -> Hallway (Ctrl+2)");
        }
        else if (!m_doorOpened)
        {
            m_tutorial->DisableAll();
            OpenHallwayDoorLayoutOnly();

            player.SetCurrentGroundLevel(HALLWAY_GROUND_LEVEL);
            player.SetPosition({ hallwaySpawnX, hallwaySpawnY });
            player.ResetVelocity();
            player.SetOnGround(true);
            m_camera.SetPosition(player.GetPosition());

            Logger::Instance().Log(Logger::Severity::Event, "Cheat: Teleported to Hallway");
        }
        else if (m_doorOpened && !m_rooftopAccessed)
        {
            m_tutorial->DisableAll();
            m_room->SetRightBoundaryActive(false);
            ApplyHallwayCameraBounds();
            player.SetCurrentGroundLevel(HALLWAY_GROUND_LEVEL);
            player.SetPosition({ hallwaySpawnX, hallwaySpawnY });
            player.ResetVelocity();
            player.SetOnGround(true);
            m_camera.SetPosition(player.GetPosition());
            Logger::Instance().Log(Logger::Severity::Event, "Cheat: Room -> Hallway (Ctrl+2, door already open)");
        }
    }

    if (input.IsKeyPressed(Input::Key::LeftControl) && input.IsKeyTriggered(Input::Key::Num3))
    {
        silenceStoryForMapCheat();
        m_tutorial->DisableAll();
        m_camera.StopAnimation();
        m_undergroundAccessed = false;
        m_trainAccessed = false;
        m_trainDeferEntryUntilIntroDone = false;
        m_cameraZoom = 1.0f;
        m_trainZoomTransition = false;
        player.SetSizeScale(1.0f);
        HandleHallwayToRooftopTransition();
        m_currentCheckpoint = MapZone::Rooftop;
        m_camera.SetPosition(player.GetPosition());
        Logger::Instance().Log(Logger::Severity::Event, "Cheat: Teleport to Rooftop (Ctrl+3)");
    }

    if (input.IsKeyPressed(Input::Key::LeftControl) && input.IsKeyTriggered(Input::Key::Num4))
    {
        silenceStoryForMapCheat();
        m_skipRooftopQHintByCheat = true;
        m_tutorial->DisableAll();
        m_camera.StopAnimation();
        m_rooftopQStoryPending = false;
        m_rooftopQStoryDelayRemaining = 0.0f;
        m_cameraZoom = 1.0f;
        m_trainZoomTransition = false;
        player.SetSizeScale(1.0f);
        if (m_trainAccessed)
        {
            m_trainAccessed = false;
            m_trainDeferEntryUntilIntroDone = false;
            m_rooftopAccessed = true;
            m_undergroundAccessed = true;
            m_currentCheckpoint = MapZone::Underground;

            float playerStartX = Underground::MIN_X + 100.0f;
            float playerStartY = Underground::MIN_Y + 270.0f;
            float newGroundLevel = Underground::MIN_Y + 75.0f;
            player.SetCurrentGroundLevel(newGroundLevel);
            player.SetPosition({ playerStartX, playerStartY });
            player.ResetVelocity();
            player.SetOnGround(false);

            m_camera.SetBounds(
                { Underground::MIN_X, Underground::MIN_Y },
                { Underground::MIN_X + Underground::WIDTH, Underground::MIN_Y + Underground::HEIGHT });
            m_cameraSmoothSpeed = 0.05f;
            m_camera.SetPosition(player.GetPosition());
            Logger::Instance().Log(Logger::Severity::Event, "Cheat: Train -> Underground (Ctrl+4)");
        }
        else
        {
            HandleRooftopToUndergroundTransition();
            m_currentCheckpoint = MapZone::Underground;
            m_camera.SetPosition(player.GetPosition());
            Logger::Instance().Log(Logger::Severity::Event, "Cheat: Teleport to Underground (Ctrl+4)");
        }
    }

    if (input.IsKeyPressed(Input::Key::LeftControl) && input.IsKeyTriggered(Input::Key::Num5))
    {
        silenceStoryForMapCheat();
        m_skipRooftopQHintByCheat = true;
        m_tutorial->DisableAll();
        m_camera.StopAnimation();
        m_cameraZoom = 1.0f;
        m_trainZoomTransition = false;
        player.SetSizeScale(1.0f);
        m_trainDeferEntryUntilIntroDone = false;
        StartTransition(PendingTransition::UndergroundToTrain);
        Logger::Instance().Log(Logger::Severity::Event, "Cheat: Teleport to Train (Ctrl+5)");
    }

    if (m_trainAccessed && m_train
        && (input.IsGlfwKeyPressed(GLFW_KEY_LEFT_ALT) || input.IsGlfwKeyPressed(GLFW_KEY_RIGHT_ALT)))
    {
        auto warpTrainCarIf = [&](Input::Key numKey, int car1to5) {
            if (!input.IsKeyTriggered(numKey))
                return;
            const float    cx = m_train->GetTrainCarCenterWorldX(car1to5);
            const Math::Vec2 p0 = player.GetPosition();
            const Math::Vec2 hc = player.GetHitboxCenter();
            player.SetPosition({ p0.x + (cx - hc.x), p0.y });
            player.ResetVelocity();
            m_train->SetTrainCarCheatUnlock(true);
            Logger::Instance().Log(Logger::Severity::Event, "Cheat: Train warp carIdx %d (Alt key)", car1to5);
        };
        warpTrainCarIf(Input::Key::Num1, 1);
        warpTrainCarIf(Input::Key::Num2, 2);
        warpTrainCarIf(Input::Key::Num3, 3);
        warpTrainCarIf(Input::Key::Num4, 5);
        warpTrainCarIf(Input::Key::Num5, 4);
    }

    const float delayTick = std::min(static_cast<float>(dt), STORY_DELAY_DT_CAP);
    if (!m_blockAmbientStoryForSession && !suppressAmbientStoryThisFrame && m_openingStoryDelayRemaining > 0.0f && m_storyDialogue && m_font && m_fontShader)
    {
        m_openingStoryDelayRemaining -= delayTick;
        if (m_openingStoryDelayRemaining <= 0.0f)
        {
            m_openingStoryDelayRemaining = 0.0f;
            m_storyDialogue->EnqueueOpening(*m_font, *m_fontShader);
        }
    }

    if (m_rooftopQStoryPending && m_storyDialogue && m_font && m_fontShader)
    {
        m_rooftopQStoryDelayRemaining -= delayTick;
        if (m_rooftopQStoryDelayRemaining <= 0.0f)
        {
            m_rooftopQStoryPending = false;
            m_rooftopQStoryDelayRemaining = 0.0f;
            m_rooftopQStoryDone = true;
            m_storyDialogue->EnqueueLines(
                { "Q skill unlocked. Press Q to release a pulse burst around you." },
                *m_font, *m_fontShader, nullptr, true, true);
        }
    }

    if (m_storyDialogue)
    {
        m_storyDialogue->Update(static_cast<float>(dt), input, ctl, *m_font, *m_fontShader);
        if (m_storyDialogue->IsBlocking())
        {
            if (!m_camera.IsAnimating())
                m_camera.Update(player.GetPosition(), m_cameraSmoothSpeed);
            SoundSystem::Instance().Update();
            return;
        }
    }

    if (input.IsKeyTriggered(Input::Key::Tab))
    {
        m_isDebugDraw = !m_isDebugDraw;
    }

    // Tutorial UI is disabled per request.

    Math::Vec2 playerCenter = player.GetPosition();
    Math::Vec2 playerHitboxSize = player.GetHitboxSize();
    Math::Vec2 playerHbCenter = player.GetHitboxCenter();

    if (!m_rooftopAccessed)
        m_prevRooftopForQHint = false;

    const bool hallwayHidingBlocksDroneAttack = m_doorOpened && !m_rooftopAccessed
        && m_hallway->IsPlayerHiding(playerCenter, playerHitboxSize, player.IsCrouching());

    bool isPressingInteract = ctl.IsActionPressed(ControlAction::PulseAbsorb, input);
    const bool isPlayerHidingInRoomForAttack =
        m_room->IsPlayerHiding(playerCenter, playerHitboxSize, player.IsCrouching());
    const bool isPlayerHidingInHallwayForAttack =
        m_hallway->IsPlayerHiding(playerCenter, playerHitboxSize, player.IsCrouching());
    const bool isPlayerHidingInUndergroundForAttack =
        m_undergroundAccessed && m_underground
        && m_underground->IsPlayerHiding(playerCenter, playerHitboxSize, player.IsCrouching());
    const bool isPlayerHidingInTrainForAttack =
        m_trainAccessed && m_train && m_train->IsPlayerHiding(playerHbCenter, playerHitboxSize, player.IsCrouching());
    const bool crouchHidingBlocksAttack =
        player.IsCrouching()
        && (isPlayerHidingInRoomForAttack || isPlayerHidingInHallwayForAttack || isPlayerHidingInUndergroundForAttack
            || isPlayerHidingInTrainForAttack);
    const bool car2PulseBoxBlocksAttack =
        m_trainAccessed && m_train
        && m_train->IsPlayerInCar2PurplePulseBox(playerHbCenter, playerHitboxSize);
    bool isPressingAttack = ctl.IsActionPressed(ControlAction::Attack, input) && !crouchHidingBlocksAttack
        && !car2PulseBoxBlocksAttack;

    // Get mouse world position
    double mouseScreenX, mouseScreenY;
    input.GetMousePosition(mouseScreenX, mouseScreenY);
    Math::Vec2 mouseWorldPos = ScreenToWorldCoordinates(mouseScreenX, mouseScreenY);
    if (!m_isGameOver && input.IsGamepadConnected())
        ApplyGamepadDroneTargetingAssist(dt, input, mouseWorldPos);
    m_lastMouseWorldPos = mouseWorldPos;

    // Auto hot-reload on file save for map object coordinates/sizes/sprites.
    if (MapObjectConfig::Instance().ReloadIfChanged())
    {
        const auto& cfg = MapObjectConfig::Instance().GetData();
        m_room->ApplyConfig(cfg.room);
        m_hallway->ApplyConfig(cfg.hallway);
        m_rooftop->ApplyConfig(cfg.rooftop);
        m_underground->ApplyConfig(cfg.underground);
        m_train->ApplyConfig(cfg.train);
    }

    auto configManager = gsm.GetEngine().GetDroneConfigManager();
    if (configManager)
    {
        configManager->ReloadConfigIfChanged();
        if (configManager->ReloadLiveStatesIfChanged())
        {
            for (size_t i = 0; i < droneManager->GetDrones().size(); ++i)
                configManager->ApplyLiveStateToDrone("Main", static_cast<int>(i), const_cast<Drone&>(droneManager->GetDrones()[i]));

            auto& hallwayDrones = m_hallway->GetDrones();
            for (size_t i = 0; i < hallwayDrones.size(); ++i)
                configManager->ApplyLiveStateToDrone("Hallway", static_cast<int>(i), hallwayDrones[i]);

            auto& rooftopDrones = m_rooftop->GetDrones();
            for (size_t i = 0; i < rooftopDrones.size(); ++i)
                configManager->ApplyLiveStateToDrone("Rooftop", static_cast<int>(i), rooftopDrones[i]);

            auto& undergroundDrones = m_underground->GetDrones();
            for (size_t i = 0; i < undergroundDrones.size(); ++i)
                configManager->ApplyLiveStateToDrone("Underground", static_cast<int>(i), undergroundDrones[i]);
            m_underground->ReapplyEntryTracerDroneAfterLiveState();

            auto& trainDrones = m_train->GetDrones();
            for (size_t i = 0; i < trainDrones.size(); ++i)
                configManager->ApplyLiveStateToDrone("Train", static_cast<int>(i), trainDrones[i]);
        }
    }

    auto robotConfigManager = gsm.GetEngine().GetRobotConfigManager();
    if (robotConfigManager && robotConfigManager->ReloadLiveStatesIfChanged())
    {
        auto& undergroundRobots = m_underground->GetRobots();
        for (size_t i = 0; i < undergroundRobots.size(); ++i)
            robotConfigManager->ApplyLiveStateToRobot("Underground", static_cast<int>(i), undergroundRobots[i]);

        auto& trainRobots = m_train->GetRobots();
        for (size_t i = 0; i < trainRobots.size(); ++i)
            robotConfigManager->ApplyLiveStateToRobot("Train", static_cast<int>(i), trainRobots[i]);
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
        m_train->GetPulseSources(), isPressingInteract, dt, mouseWorldPos);

    Drone* targetDrone = nullptr;
    Robot* targetRobot = nullptr;
    float side = 1.0f;

    auto GetHorizontalRelationByAABB = [](Math::Vec2 playerCenter, Math::Vec2 playerSize,
        Math::Vec2 targetCenter, Math::Vec2 targetSize) -> float
        {
            float playerLeft = playerCenter.x - playerSize.x * 0.5f;
            float targetLeft = targetCenter.x - targetSize.x * 0.5f;

            return (playerLeft < targetLeft) ? 1.0f : -1.0f;
        };
    if (!isPressingAttack)
    {
        m_lockedAttackDrone = nullptr;
        m_lockedAttackRobot = nullptr;
        m_lockedAttackSide = 1.0f;
    }

    if (isPressingAttack)
    {
        if (m_lockedAttackDrone && !m_lockedAttackDrone->IsDead() && !m_lockedAttackDrone->IsHit())
        {
            targetDrone = m_lockedAttackDrone;
            targetRobot = nullptr;

            side = m_lockedAttackSide;
        }
        else if (m_lockedAttackRobot && !m_lockedAttackRobot->IsDead())
        {
            targetRobot = m_lockedAttackRobot;
            targetDrone = nullptr;

            side = m_lockedAttackSide;
        }
        else
        {
            m_lockedAttackDrone = nullptr;
            m_lockedAttackRobot = nullptr;

            auto* imguiManager = gsm.GetEngine().GetImguiManager();
            bool isGodMode = imguiManager && imguiManager->IsPlayerGodMode();

            if (isGodMode || player.GetPulseCore().getPulse().Value() > PULSE_COST_PER_SECOND * dt)
            {
                float closestDistSq = ATTACK_RANGE_SQ;

                auto checkDrones = [&](std::vector<Drone>& drones)
                    {
                        const Math::Vec2 cursorHitbox = { 32.0f, 32.0f };

                        for (auto& drone : drones)
                        {
                            if (drone.IsDead() || drone.IsHit())
                                continue;

                            float distSq = (playerCenter - drone.GetPosition()).LengthSq();

                            Math::Vec2 droneHitboxSize =
                                drone.GetSize() *
                                (input.IsGamepadConnected()
                                    ? kGamepadDroneAimHitboxScale
                                    : 0.8f);

                            float droneHitboxRadius =
                                (droneHitboxSize.x + droneHitboxSize.y) * 0.25f;

                            float effectiveAttackRange =
                                ATTACK_RANGE + droneHitboxRadius;

                            float effectiveAttackRangeSq =
                                effectiveAttackRange * effectiveAttackRange;

                            bool isMouseOnDrone =
                                Collision::CheckPointInAABB(
                                    mouseWorldPos,
                                    drone.GetPosition(),
                                    droneHitboxSize)
                                ||
                                Collision::CheckAABB(
                                    mouseWorldPos,
                                    cursorHitbox,
                                    drone.GetPosition(),
                                    droneHitboxSize);

                            if (distSq < effectiveAttackRangeSq &&
                                distSq < closestDistSq &&
                                isMouseOnDrone)
                            {
                                closestDistSq = distSq;

                                targetDrone = &drone;
                                targetRobot = nullptr;
                            }
                        }
                    };

                checkDrones(droneManager->GetDrones());

                if (!hallwayHidingBlocksDroneAttack)
                    checkDrones(m_hallway->GetDrones());

                checkDrones(m_rooftop->GetDrones());

                if (m_undergroundAccessed)
                    checkDrones(m_underground->GetDrones());

                if (m_trainAccessed)
                    checkDrones(m_train->GetDrones());

                if (m_trainAccessed &&
                    m_train &&
                    m_train->GetCarTransportDroneManager())
                {
                    checkDrones(
                        m_train->GetCarTransportDroneManager()->GetDrones());
                }

                if (m_trainAccessed &&
                    m_train &&
                    m_train->GetSirenDroneManager())
                {
                    checkDrones(
                        m_train->GetSirenDroneManager()->GetDrones());
                }

                if (m_undergroundAccessed)
                {
                    auto& robots = m_underground->GetRobots();

                    const Math::Vec2 cursorHitbox = { 32.0f, 32.0f };

                    for (auto& robot : robots)
                    {
                        if (robot.IsDead())
                            continue;

                        float distSq =
                            (playerCenter - robot.GetPosition()).LengthSq();

                        Math::Vec2 robotSize = robot.GetSize();

                        float robotRadius =
                            (robotSize.x + robotSize.y) * 0.25f;

                        float effectiveAttackRange =
                            ATTACK_RANGE + robotRadius;

                        float effectiveAttackRangeSq =
                            effectiveAttackRange * effectiveAttackRange;

                        bool isMouseOnRobot =
                            Collision::CheckPointInAABB(
                                mouseWorldPos,
                                robot.GetPosition(),
                                robotSize)
                            ||
                            Collision::CheckAABB(
                                mouseWorldPos,
                                cursorHitbox,
                                robot.GetPosition(),
                                robotSize);

                        if (distSq < effectiveAttackRangeSq &&
                            distSq < closestDistSq &&
                            isMouseOnRobot)
                        {
                            closestDistSq = distSq;

                            targetRobot = &robot;
                            targetDrone = nullptr;
                        }
                    }
                }

                if (m_trainAccessed)
                {
                    auto& robots = m_train->GetRobots();

                    const Math::Vec2 cursorHitbox = { 32.0f, 32.0f };

                    for (auto& robot : robots)
                    {
                        if (robot.IsDead())
                            continue;

                        float distSq =
                            (playerCenter - robot.GetPosition()).LengthSq();

                        Math::Vec2 robotSize = robot.GetSize();

                        float robotRadius =
                            (robotSize.x + robotSize.y) * 0.25f;

                        float effectiveAttackRange =
                            ATTACK_RANGE + robotRadius;

                        float effectiveAttackRangeSq =
                            effectiveAttackRange * effectiveAttackRange;

                        bool isMouseOnRobot =
                            Collision::CheckPointInAABB(
                                mouseWorldPos,
                                robot.GetPosition(),
                                robotSize)
                            ||
                            Collision::CheckAABB(
                                mouseWorldPos,
                                cursorHitbox,
                                robot.GetPosition(),
                                robotSize);

                        if (distSq < effectiveAttackRangeSq &&
                            distSq < closestDistSq &&
                            isMouseOnRobot)
                        {
                            closestDistSq = distSq;

                            targetRobot = &robot;
                            targetDrone = nullptr;
                        }
                    }
                }
            }
            if (targetDrone)
            {
                m_lockedAttackDrone = targetDrone;
                m_lockedAttackRobot = nullptr;

                Math::Vec2 lockTargetCenter = DronePulseTargetCenter(*targetDrone);
                Math::Vec2 lockTargetSize = targetDrone->GetSize() * 0.8f;

                m_lockedAttackSide = GetHorizontalRelationByAABB(
                    playerCenter,
                    playerHitboxSize,
                    lockTargetCenter,
                    lockTargetSize
                );
            }
            else if (targetRobot)
            {
                m_lockedAttackRobot = targetRobot;
                m_lockedAttackDrone = nullptr;

                Math::Vec2 lockTargetCenter = targetRobot->GetPosition();
                Math::Vec2 lockTargetSize = targetRobot->GetSize();

                m_lockedAttackSide = GetHorizontalRelationByAABB(
                    playerCenter,
                    playerHitboxSize,
                    lockTargetCenter,
                    lockTargetSize
                );
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
        Math::Vec2 targetCenter;
        Math::Vec2 targetSize;

        float playerInset = m_trainAccessed ? TRAIN_PLAYER_INSET : PLAYER_INSET;
        float droneInset = m_trainAccessed ? TRAIN_DRONE_TARGET_INSET : DRONE_TARGET_INSET;

        float robotCoreInsetX = m_trainAccessed ? TRAIN_ROBOT_CORE_INSET_X : ROBOT_CORE_INSET_X;
        float robotCoreOffsetY = m_trainAccessed ? TRAIN_ROBOT_CORE_OFFSET_Y : ROBOT_CORE_OFFSET_Y;

        constexpr float KILL_PULSE_REWARD = 10.0f;

        if (targetDrone != nullptr)
        {
            // Cache position before potential reinforcement spawn.
            // OnDroneKilled may spawn drones and reallocate vectors, invalidating targetDrone pointer.

            targetSize = targetDrone->GetSize() * 0.8f;
            targetCenter = DronePulseTargetCenter(*targetDrone);

            float targetSide = m_lockedAttackSide;

            targetPos = {
                targetCenter.x - targetSide * (targetSize.x * 0.5f - droneInset),
                targetCenter.y
            };

            side = targetSide;

            bool didDroneDie = targetDrone->ApplyDamage(static_cast<float>(dt));
            if (didDroneDie)
            {
                m_traceSystem->OnDroneKilled(*droneManager, player.GetPosition());
                player.GetPulseCore().getPulse().add(KILL_PULSE_REWARD);
            }
        }
        else if (targetRobot != nullptr)
        {
            float damagePerSecond = 30.0f;

            if (Collision::CheckAABB(playerCenter, playerHitboxSize, targetRobot->GetPosition(), targetRobot->GetSize()))
            {
                damagePerSecond = 50.0f;
            }

            bool wasAlive = !targetRobot->IsDead();
            targetRobot->TakeDamage(damagePerSecond * static_cast<float>(dt));

            targetSize = targetRobot->GetSize();
            targetCenter = targetRobot->GetPosition();

            float targetSide = m_lockedAttackSide;

            targetPos = {
                targetCenter.x - targetSide * (targetSize.x * 0.5f - robotCoreInsetX),
                targetCenter.y + robotCoreOffsetY
            };

            side = targetSide;

            if (wasAlive && targetRobot->IsDead())
            {
                player.GetPulseCore().getPulse().add(KILL_PULSE_REWARD);
            }
        }
        float playerHalfW = playerHitboxSize.x * 0.5f;

        float safePlayerInset = std::min(playerInset, playerHalfW - 2.0f);
        safePlayerInset = std::max(0.0f, safePlayerInset);

        float playerAttachDist = playerHalfW - safePlayerInset;

        Math::Vec2 vfxStartPos = {
            playerCenter.x + side * playerAttachDist,
            playerCenter.y
        };

        pulseManager->UpdateAttackVFX(true, vfxStartPos, targetPos, side);

        m_door->Update(player, false, mouseWorldPos);
        m_rooftopDoor->Update(player, false, mouseWorldPos);
    }
    else
    {
        pulseManager->UpdateAttackVFX(false, {}, {});

        if (ctl.IsActionTriggered(ControlAction::Attack, input))
        {
            if (m_room->IsBlindOpen())
                m_door->Update(player, true, mouseWorldPos);
            else
                m_door->Update(player, false, mouseWorldPos);

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
        StartTransition(PendingTransition::RoomToHallway);
    }

    if (m_rooftopDoor->ShouldLoadNextMap() && !m_rooftopAccessed)
    {
        StartTransition(PendingTransition::HallwayToRooftop);
    }

    if (m_rooftopAccessed && !m_undergroundAccessed)
    {
        float playerRight = player.GetPosition().x + player.GetHitboxSize().x * 0.5f;
        float transitionX = Rooftop::MIN_X + Rooftop::WIDTH - 1.0f;
        if (playerRight >= transitionX)
        {
            StartTransition(PendingTransition::RooftopToUnderground);
        }
    }

    if (m_undergroundAccessed && !m_trainAccessed)
    {
        float transitionX = Underground::MIN_X + Underground::WIDTH - 50.0f;
        if (player.GetPosition().x > transitionX)
        {
            StartTransition(PendingTransition::UndergroundToTrain);
        }
    }

    bool isPlayerHidingInRoom = m_room->IsPlayerHiding(playerCenter, playerHitboxSize, player.IsCrouching());
    bool isPlayerHidingInHallway = m_hallway->IsPlayerHiding(playerCenter, playerHitboxSize, player.IsCrouching());
    bool isPlayerHiding = isPlayerHidingInRoom || isPlayerHidingInHallway;
    if (m_trainAccessed && m_train)
        isPlayerHiding = isPlayerHiding
            || m_train->IsPlayerHiding(playerHbCenter, playerHitboxSize, player.IsCrouching());

    player.SetHiding(isPlayerHiding);
    if (!m_trainAccessed)
        player.SetTrainEnemyUndetectable(false);

    float tracerTrainAssist = 1.f;
    if (m_trainAccessed && m_train)
    {
        constexpr float refSp = Train::TRAIN_SPEED;
        const float     ratio =
            (refSp > 0.f) ? std::clamp(m_train->GetTrainCurrentSpeed() / refSp, 0.f, 1.f) : 0.f;
        tracerTrainAssist = 1.f + 0.22f * ratio;
    }
    bool mainMapDroneUndetectable = isPlayerHiding;
    if (m_trainAccessed && m_train)
        mainMapDroneUndetectable = mainMapDroneUndetectable
            || m_train->IsPlayerInCar2PurplePulseBox(playerHbCenter, playerHitboxSize);
    droneManager->Update(dt, player, playerHitboxSize, mainMapDroneUndetectable, true, 1.f, tracerTrainAssist);

    if (m_trainAccessed)
    {
        for (auto& d : droneManager->GetDrones())
        {
            if (d.IsDead() || !d.IsTracer())
                continue;
            // 기본 텍스처 폭 ≈120 — 이미 기차 전투용으로 줄인 드론(~90)은 건너뜀
            if (d.GetSize().x > 110.f)
                Train::ApplyCombatDroneVisualScale(d);
        }
    }

    if (m_rooftopAccessed && !m_undergroundAccessed && !m_trainAccessed)
    {
        m_rooftop->SyncGroundLevelForPlayer(player, playerHitboxSize);
    }
    else if (m_doorOpened && !m_rooftopAccessed)
    {
        player.SetCurrentGroundLevel(HALLWAY_GROUND_LEVEL);
    }

    player.Update(dt, input, ctl);

    if (m_doorOpened)
    {
        float leftLimit = GAME_WIDTH + player.GetHitboxSize().x * 0.5f + HALLWAY_ENTRY_MARGIN_X;

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
        m_room->Update(player, dt, input, mouseWorldPos, ctl);

        if (m_storyDialogue && m_room && !m_blockAmbientStoryForSession && !suppressAmbientStoryThisFrame)
        {
            if (m_room->IsBlindOpen() && !m_wasBlindOpen)
            {
                m_storyDialogue->EnqueueLines(
                    {
                        "It's getting dark... but the whole city is already blacked out.",
                        "I need to get out of here.",
                    },
                    *m_font,
                    *m_fontShader);
            }
            m_wasBlindOpen = m_room->IsBlindOpen();

            if (m_room->ConsumeBlindInteractDenied())
            {
                if (!m_roomBlindLowPulseStoryDone && !m_storyDialogue->IsBlocking())
                {
                    m_storyDialogue->EnqueueLines(
                        {
                            "It won't open.",
                            "Maybe it needs energy... or pulse.",
                            "I should find something electrical to recharge from.",
                        },
                        *m_font,
                        *m_fontShader,
                        [this] { m_roomBlindLowPulseStoryDone = true; });
                }
            }
        }
    }

    auto& pp = engine.GetPostProcess();
    pp.Settings().cameraPos = m_camera.GetPosition();

    if (m_doorOpened && !m_rooftopAccessed)
    {
        pp.Settings().exposure = 0.4f;
        pp.Settings().useLightOverlay = true;
        pp.Settings().lightOverlayStrength = 0.6f;
        pp.Settings().cameraPos = m_camera.GetPosition();
    }
    else if (m_rooftopAccessed && !m_undergroundAccessed)
    {
        // rooftop
        pp.Settings().exposure = 0.85f; 
        pp.Settings().useLightOverlay = false;
        pp.Settings().lightOverlayStrength = 0.0f;
    }
    else
    {
        pp.Settings().exposure = 1.0f;
        pp.Settings().useLightOverlay = false;
        pp.Settings().lightOverlayStrength = 0.0f;
        pp.Settings().cameraPos = m_camera.GetPosition();
    }

    m_hallway->Update(dt, playerCenter, playerHitboxSize, player, isPlayerHiding);

    if (m_storyDialogue && m_font && m_fontShader && m_doorOpened && !m_rooftopAccessed && m_hallway
        && !m_hallwayFaradayBoxStoryDone && !m_storyDialogue->IsBlocking() && !m_blockAmbientStoryForSession
        && !suppressAmbientStoryThisFrame)
    {
        const Math::Vec2 playerPos = player.GetPosition();
        bool inHidingPromptRange = false;
        for (const auto& spot : m_hallway->GetHidingSpots())
        {
            const Math::Vec2 topCenter = {
                spot.pos.x,
                spot.pos.y + spot.size.y * 0.5f + HIDING_S_ICON_OFFSET_Y
            };
            if ((playerPos - topCenter).LengthSq() <= HIDING_S_PROMPT_DISTANCE_SQ)
            {
                inHidingPromptRange = true;
                break;
            }
        }
        if (inHidingPromptRange)
        {
            m_hallwayFaradayBoxStoryDone = true;
            m_storyDialogue->EnqueueLines(
                { "Wait, a Faraday box? This might actually keep us off their radar. Time to disappear." },
                *m_font,
                *m_fontShader);
        }
    }

    m_rooftop->Update(dt, player, playerHitboxSize, input, mouseWorldPos,
                      ctl.IsActionTriggered(ControlAction::Attack, input));

    auto runPulseResonanceBurst = [&]() {
        const bool isGodMode = [&]() -> bool {
            auto* im = gsm.GetEngine().GetImguiManager();
            return im && im->IsPlayerGodMode();
        }();
        std::vector<std::pair<Math::Vec2, Math::Vec2>> trainStaticChainArcs;
        if (m_trainAccessed && m_train)
            m_train->AppendCarTransportStraightChainArcs(trainStaticChainArcs);

        // 최신 히트박스 중심 사용. Train은 m_train->Update에서 캐리 후에 호출해야 원이 플레이어와 일치.
        m_pulseDetonateSkill.Update(dt, player, player.GetHitboxCenter(),
            m_rooftop->GetDroneManager(),
            (m_undergroundAccessed && m_underground) ? m_underground->GetDroneManager() : nullptr,
            droneManager.get(),
            *pulseManager, ctl, input,
            m_isGameOver, m_rooftopAccessed || m_undergroundAccessed, isGodMode,
            (m_trainAccessed && m_train) ? m_train->GetDroneManager() : nullptr,
            (m_trainAccessed && m_train) ? m_train->GetSirenDroneManager() : nullptr,
            &trainStaticChainArcs,
            (m_trainAccessed && m_train) ? m_train->GetCarTransportDroneManager() : nullptr,
            (m_trainAccessed && m_train) ? m_train.get() : nullptr,
            (m_undergroundAccessed && m_underground) ? m_underground.get() : nullptr);
    };

    if (!m_trainAccessed)
        runPulseResonanceBurst();

    if (m_storyDialogue && m_rooftopAccessed && !m_undergroundAccessed && m_rooftop && !m_blockAmbientStoryForSession
        && !suppressAmbientStoryThisFrame)
    {
        const bool nearLift = m_rooftop->IsPlayerNearLift();
        if (nearLift && !m_wasNearLiftRooftop && !m_rooftopLiftStoryDone)
        {
            m_storyDialogue->EnqueueLines(
                { "There's no way across unless I activate that lift." },
                *m_font,
                *m_fontShader);
            m_rooftopLiftStoryDone = true;
        }
        m_wasNearLiftRooftop = nearLift;
    }

    if (m_undergroundAccessed)
    {
        m_underground->Update(dt, player, playerHitboxSize);
    }

    if (m_trainAccessed)
    {
        const bool trainCarInjectGodMode = [&]() -> bool {
            auto* im = gsm.GetEngine().GetImguiManager();
            return im && im->IsPlayerGodMode();
        }();
        const bool attackHeld = ctl.IsActionPressed(ControlAction::Attack, input);
        const bool attackTriggered = ctl.IsActionTriggered(ControlAction::Attack, input);
        int        carInjectForced = -1;
        if (m_train)
            carInjectForced =
                m_train->TryGetCarTransportStartInjectSlot(player.GetHitboxCenter(), playerHitboxSize, mouseWorldPos);
        const bool injectViaStartIcon = carInjectForced >= 0 && attackHeld;
        const bool carTransportInjectHeld = isPressingInteract || injectViaStartIcon;
        m_train->Update(dt, player, playerHitboxSize, isPressingInteract, carTransportInjectHeld, trainCarInjectGodMode,
                        attackHeld, attackTriggered, mouseWorldPos, injectViaStartIcon ? carInjectForced : -1);

        runPulseResonanceBurst();

        const float shakePx = m_train->ConsumeTrainCameraShakeRequest();
        if (shakePx > 0.f)
            m_camera.AddScreenShake(0.48f, shakePx);

        // Keep right bound expanding while player advances.
        // This prevents camera lock when the player leaves the train and keeps moving right.
        const float visibleW = GAME_WIDTH / m_cameraZoom;
        const float playerLeadMargin = visibleW * 0.75f;
        const float trainDrivenRight = m_train->GetEffectiveRightBound();
        const float playerDrivenRight = player.GetPosition().x + playerLeadMargin;
        const float dynamicRight = (trainDrivenRight > playerDrivenRight) ? trainDrivenRight : playerDrivenRight;

        // Match Camera::Update: it clamps using view half-height (GAME_HEIGHT/2), not zoom.
        // Third_ThirdTrain (car index 4) only: allow vertical follow on container stacks / upper pipe.
        constexpr float trainCamHalfH = GAME_HEIGHT * 0.5f;
        const float     py              = player.GetPosition().y;
        float           boundMinY       = Train::MIN_Y;
        float           boundMaxY       = Train::MIN_Y + Train::HEIGHT;
        if (m_train->GetPlayerTrainCarIndex(playerHbCenter) == 4)
        {
            boundMinY = std::min(Train::MIN_Y, py - trainCamHalfH);
            boundMaxY = std::max(Train::MIN_Y + Train::HEIGHT, py + trainCamHalfH);
        }

        m_camera.SetBounds({ Train::MIN_X, boundMinY }, { dynamicRight, boundMaxY });
    }

    auto& hallwayDrones = m_hallway->GetDrones();
    for (auto& drone : hallwayDrones)
    {
        if (!drone.IsDead() && drone.ShouldDealDamage())
        {
            if (!isPlayerHidingInHallway)
            {
                auto* imguiManager = gsm.GetEngine().GetImguiManager();
                if (!imguiManager || !imguiManager->IsPlayerGodMode())
                {
                    player.TakeDamage(10.0f);
                }
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
        const bool isPlayerHidingInUnderground =
            m_underground->IsPlayerHiding(playerCenter, playerHitboxSize, player.IsCrouching());
        auto& undergroundDrones = m_underground->GetDrones();
        for (auto& drone : undergroundDrones)
        {
            if (!drone.IsDead() && drone.ShouldDealDamage())
            {
                if (!isPlayerHidingInUnderground)
                {
                    auto* imguiManager = gsm.GetEngine().GetImguiManager();
                    if (!imguiManager || !imguiManager->IsPlayerGodMode())
                    {
                        player.TakeDamage(20.0f);
                    }
                }
                drone.ResetDamageFlag();
                break;
            }
        }
    }

    if (m_trainAccessed)
    {
        const bool isPlayerHidingInTrain =
            m_train->IsPlayerHiding(playerHbCenter, playerHitboxSize, player.IsCrouching());

        auto&     trainDrones      = m_train->GetDrones();
        const int trainPlayerCar   = m_train->GetPlayerTrainCarIndex(playerHbCenter);
        for (auto& drone : trainDrones)
        {
            if (!drone.IsDead() && drone.ShouldDealDamage())
            {
                if (!isPlayerHidingInTrain)
                {
                    auto* imguiManager = gsm.GetEngine().GetImguiManager();
                    if (!imguiManager || !imguiManager->IsPlayerGodMode())
                    {
                        player.TakeDamage(25.0f);
                        if (m_train)
                            m_train->NotifyCarTransportInjectionInterrupted();
                    }
                }
                drone.ResetDamageFlag();
                break;
            }
        }

        if (m_train->GetCarTransportDroneManager())
        {
            auto& ctDrones = m_train->GetCarTransportDroneManager()->GetDrones();
            for (auto& drone : ctDrones)
            {
                if (!drone.IsDead() && drone.ShouldDealDamage())
                {
                    if (trainPlayerCar != 4)
                    {
                        drone.ResetDamageFlag();
                        continue;
                    }
                    if (!isPlayerHidingInTrain)
                    {
                        auto* imguiManager = gsm.GetEngine().GetImguiManager();
                        if (!imguiManager || !imguiManager->IsPlayerGodMode())
                        {
                            player.TakeDamage(25.0f);
                            if (m_train)
                                m_train->NotifyCarTransportInjectionInterrupted();
                        }
                    }
                    drone.ResetDamageFlag();
                    break;
                }
            }
        }

        if (m_train->GetSirenDroneManager())
        {
            auto& sirenDrones = m_train->GetSirenDroneManager()->GetDrones();
            for (auto& drone : sirenDrones)
            {
                if (!drone.IsDead() && drone.ShouldDealDamage())
                {
                    if (!m_train->IsSirenDroneDamageBlocked(playerHbCenter, playerHitboxSize, player.IsCrouching()))
                    {
                        auto* imguiManager = gsm.GetEngine().GetImguiManager();
                        if (!imguiManager || !imguiManager->IsPlayerGodMode())
                        {
                            player.TakeDamage(25.0f);
                            if (m_train)
                                m_train->NotifyCarTransportInjectionInterrupted();
                        }
                    }
                    drone.ResetDamageFlag();
                    break;
                }
            }
        }
    }

    if (pulseManager)
        pulseManager->SyncDetonationOriginToPlayer(player.GetHitboxCenter());

    // Update camera animation
    if (m_camera.IsAnimating())
    {
        m_camera.UpdateAnimation(static_cast<float>(dt));
    }
    else
    {
        m_camera.Update(player.GetPosition(), m_cameraSmoothSpeed);
    }
    m_camera.UpdateScreenShake(static_cast<float>(dt));

    if (m_hallwayEntryStoryPending && m_storyDialogue && m_font && m_fontShader && !m_blockAmbientStoryForSession
        && !suppressAmbientStoryThisFrame)
    {
        if (m_hallwayEntryStoryDelayRemaining > 0.0f)
            m_hallwayEntryStoryDelayRemaining -= delayTick;
        if (m_hallwayEntryStoryDelayRemaining <= 0.0f)
        {
            m_hallwayEntryStoryPending = false;
            m_hallwayEntryStoryDelayRemaining = 0.0f;
            m_storyDialogue->EnqueueLines(
                {
                    "There's a drone. I should avoid it for now.",
                    "But... maybe I can use this power against it.",
                    "First, I need to find somewhere to charge.",
                },
                *m_font,
                *m_fontShader);
        }
    }

    static double cameraLogTimer = 0.0f;
    cameraLogTimer += dt;
    // 모든 맵에서 동일하게 좌표 로그 (루프탑 전용이면 Train/언더그라운드·치트 이동 시 콘솔이 비어 보임)
    if (cameraLogTimer > 1.0f)
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
        const double rawFps = (m_fpsTimer > 0.0) ? (m_frameCount / m_fpsTimer) : 0.0;
        int average_fps = static_cast<int>(std::ceil(rawFps - 0.0001));

        const Engine& engineRef = gsm.GetEngine();
        const int cap = engineRef.GetFpsCap();
        if (!engineRef.IsVSyncEnabled() && cap > 0)
        {
            if (average_fps > cap) average_fps = cap;
        }

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

    m_pulseDetonateSkill.UpdateCooldownText(*m_font, *m_fontShader);

    std::stringstream ss_warning;
    ss_warning << "Warning Level: " << m_traceSystem->GetWarningLevel();
    m_warningLevelText = m_font->PrintToTexture(*m_fontShader, ss_warning.str());

    if (engine.GetImguiManager())
    {
        engine.GetImguiManager()->SetWarningLevel(m_traceSystem->GetWarningLevel());
    }

    if (player.IsDead())
    {
        // Sync checkpoint with the currently active map before opening GameOver.
        if (m_trainAccessed)
            m_currentCheckpoint = MapZone::Train;
        else if (m_undergroundAccessed)
            m_currentCheckpoint = MapZone::Underground;
        else if (m_rooftopAccessed)
            m_currentCheckpoint = MapZone::Rooftop;
        else if (m_doorOpened)
            m_currentCheckpoint = MapZone::Hallway;
        else
            m_currentCheckpoint = MapZone::Room;

        engine.GetPostProcess().Settings().exposure = 1.0f;
        gsm.PushState(std::make_unique<GameOver>(gsm, m_isGameOver,
            [this]() { RespawnAtCheckpoint(); }));
        return;
    }

    SoundSystem::Instance().Update();
}

void GameplayState::ApplyHallwayCameraBounds()
{
    float cameraViewWidth = GAME_WIDTH;
    float roomToShow = cameraViewWidth * 0.20f;
    float newMinWorldX = GAME_WIDTH - roomToShow;
    float worldMaxX = GAME_WIDTH + Hallway::WIDTH;
    float worldMaxY = GAME_HEIGHT;

    m_camera.SetBounds({ newMinWorldX, 0.0f }, { worldMaxX, worldMaxY });
    m_cameraSmoothSpeed = 0.1f;
}

void GameplayState::OpenHallwayDoorLayoutOnly()
{
    m_door->ResetMapTransition();
    m_doorOpened = true;
    m_room->SetRightBoundaryActive(false);
    ApplyHallwayCameraBounds();
}

void GameplayState::HandleRoomToHallwayTransition()
{
    Logger::Instance().Log(Logger::Severity::Event, "Door opened! Hallway accessible.");
    OpenHallwayDoorLayoutOnly();
    m_hallwayEntryStoryPending = true;
    m_hallwayEntryStoryDelayRemaining = HALLWAY_ENTRY_STORY_DELAY_SEC;
    m_currentCheckpoint = MapZone::Hallway;
}

void GameplayState::HandleHallwayToRooftopTransition()
{
    m_rooftopDoor->ResetMapTransition();
    m_rooftopAccessed = true;
    m_currentCheckpoint = MapZone::Rooftop;
    if (!m_rooftopQStoryDone)
    {
        m_rooftopQStoryPending = true;
        m_rooftopQStoryDelayRemaining = 0.5f;
    }

    float newGroundLevel = Rooftop::FLOOR_SURFACE_Y;
    player.SetCurrentGroundLevel(newGroundLevel);

    float playerStartX = Rooftop::MIN_X + 1550.0f;
    float playerStartY = Rooftop::MIN_Y + (Rooftop::HEIGHT / 2.0f + 200.0f) + 50.0f;

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
    m_currentCheckpoint = MapZone::Underground;

    m_rooftop->ClearAllDrones();

    float playerStartX = Underground::MIN_X + 100.0f;
    float playerStartY = Underground::MIN_Y + 270.0f;

    float newGroundLevel = Underground::MIN_Y + 75.0f;
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

void GameplayState::HandleUndergroundToTrainTransition()
{
    Logger::Instance().Log(Logger::Severity::Event,
        "Transition to Train! Starting descent animation...");

    m_rooftopAccessed = true;
    m_undergroundAccessed = true;
    m_trainAccessed = true;
    m_currentCheckpoint = MapZone::Train;

    m_underground->ClearAllDrones();

    float playerStartX = Train::MIN_X + 300.0f;
    float playerStartY = Train::MIN_Y + 540.0f;
    float newGroundLevel = Train::MIN_Y + 90.0f;

    player.SetCurrentGroundLevel(newGroundLevel);
    player.SetPosition({ playerStartX, playerStartY });
    player.ResetVelocity();
    player.SetOnGround(false);

    float worldMinX = Train::MIN_X;
    float worldMaxX = Train::MIN_X + m_train->GetMapWidth();
    float worldMinY = Train::MIN_Y;
    float worldMaxY = Train::MIN_Y + Train::HEIGHT;

    m_camera.SetBounds({ worldMinX, worldMinY }, { worldMaxX, worldMaxY });

    // Snap camera directly to train-map center — no pan animation, only zoom effect
    Math::Vec2 cameraTargetPos = { Train::MIN_X + GAME_WIDTH / 2.0f, Train::MIN_Y + GAME_HEIGHT / 2.0f };
    m_camera.SetPosition(cameraTargetPos);

    // Start zoom-out + player shrink transition: begins zoomed in, eases out to normal
    m_cameraZoom            = TRAIN_ZOOM_START;
    m_trainZoomTransition  = true;
    m_trainZoomTimer       = 0.0f;
    player.SetSizeScale(1.0f); // will animate toward 0.6 over TRAIN_ZOOM_DURATION

    m_cameraSmoothSpeed = 0.05f;

    // 출발 카운트다운은 페이드 인 + 카메라 줌(플레이어 축소)이 끝난 뒤 시작
    m_trainDeferEntryUntilIntroDone = true;

    Logger::Instance().Log(Logger::Severity::Event,
        "Train Transition! Camera snapped to (%.1f, %.1f), Player: (%.1f, %.1f)",
        cameraTargetPos.x, cameraTargetPos.y, playerStartX, playerStartY);
}

void GameplayState::StartTransition(PendingTransition t)
{
    if (m_fadeState != FadeState::None) return;
    m_pendingTransition = t;
    m_fadeAlpha = 0.0f;
    m_fadeState = FadeState::FadingOut;
}

void GameplayState::ExecutePendingTransition()
{
    switch (m_pendingTransition)
    {
    case PendingTransition::RoomToHallway:
        HandleRoomToHallwayTransition();
        {
            const float hallwaySpawnX = GAME_WIDTH + player.GetHitboxSize().x * 0.5f + HALLWAY_ENTRY_MARGIN_X;
            player.SetPosition({ hallwaySpawnX, HALLWAY_ENTRY_POS_Y });
            player.ResetVelocity();
            m_camera.SetPosition(player.GetPosition());
        }
        break;
    case PendingTransition::HallwayToRooftop:
        HandleHallwayToRooftopTransition();
        break;
    case PendingTransition::RooftopToUnderground:
        HandleRooftopToUndergroundTransition();
        break;
    case PendingTransition::UndergroundToTrain:
        HandleUndergroundToTrainTransition();
        break;
    default:
        break;
    }
    m_pendingTransition = PendingTransition::None;
    m_fadeState = FadeState::FadingIn;
}

void GameplayState::RespawnAtCheckpoint()
{
    player.Revive(50.0f);
    m_storyDialogue->ResetForNewRun();

    // Reset all enemies
    droneManager->ResetAllDrones();
    droneManager->ClearTraceReinforcementDrones();
    if (m_traceSystem)
        m_traceSystem->Reset();
    if (m_hallway)
    {
        m_hallway->GetDroneManager()->ResetAllDrones();
        m_hallway->GetDroneManager()->ClearTraceReinforcementDrones();
    }
    if (m_rooftop)
    {
        m_rooftop->GetDroneManager()->ResetAllDrones();
        m_rooftop->GetDroneManager()->ClearTraceReinforcementDrones();
    }
    if (m_underground)
    {
        m_underground->GetDroneManager()->ResetAllDrones();
        m_underground->GetDroneManager()->ClearTraceReinforcementDrones();
        for (auto& robot : m_underground->GetRobots()) robot.Reset();
    }
    if (m_train)
    {
        m_train->GetDroneManager()->ResetAllDrones();
        m_train->GetDroneManager()->ClearTraceReinforcementDrones();
        if (m_train->GetCarTransportDroneManager())
        {
            m_train->GetCarTransportDroneManager()->ResetAllDrones();
            m_train->GetCarTransportDroneManager()->ClearTraceReinforcementDrones();
        }
        if (m_train->GetSirenDroneManager())
        {
            m_train->GetSirenDroneManager()->ResetAllDrones();
            m_train->GetSirenDroneManager()->ClearTraceReinforcementDrones();
        }
        for (auto& robot : m_train->GetRobots()) robot.Reset();
    }

    m_pulseDetonateSkill.ResetCooldown(); // unlock persists

    // Reset camera zoom and player scale (will be set per-zone below)
    m_cameraZoom = 1.0f;
    m_trainZoomTransition = false;
    m_camera.StopAnimation();

    switch (m_currentCheckpoint)
    {
    case MapZone::Room:
    {
        player.SetSizeScale(1.0f);
        player.SetCurrentGroundLevel(GROUND_LEVEL);
        player.SetPosition({
            ROOM_LEFT_BOUNDARY + player.GetSize().x * 0.5f,
            GROUND_LEVEL + player.GetSize().y * 0.5f
        });
        player.SetOnGround(true);
        m_camera.SetBounds({ 0.0f, 0.0f }, { GAME_WIDTH, GAME_HEIGHT });
        m_camera.Update(player.GetPosition(), 1.0f);
        Logger::Instance().Log(Logger::Severity::Event, "Checkpoint respawn: Room");
        break;
    }
    case MapZone::Hallway:
    {
        player.SetSizeScale(1.0f);
        const float hallwaySpawnX = GAME_WIDTH + player.GetHitboxSize().x * 0.5f + HALLWAY_ENTRY_MARGIN_X;
        const float hallwaySpawnY = HALLWAY_GROUND_LEVEL + player.GetSize().y * 0.5f;
        player.SetCurrentGroundLevel(HALLWAY_GROUND_LEVEL);
        player.SetPosition({ hallwaySpawnX, hallwaySpawnY });
        player.SetOnGround(false);
        ApplyHallwayCameraBounds();
        m_camera.SetPosition(player.GetPosition());
        if (m_hallway)
            m_hallway->RefillPulseSourcesAfterCheckpointRespawn();
        Logger::Instance().Log(Logger::Severity::Event, "Checkpoint respawn: Hallway");
        break;
    }
    case MapZone::Rooftop:
    {
        player.SetSizeScale(1.0f);
        float playerStartX = Rooftop::MIN_X + 1550.0f;
        float playerStartY = Rooftop::MIN_Y + (Rooftop::HEIGHT / 2.0f + 200.0f) + 50.0f;
        player.SetCurrentGroundLevel(Rooftop::FLOOR_SURFACE_Y);
        player.SetPosition({ playerStartX, playerStartY });
        player.SetOnGround(false);
        m_camera.SetBounds({ Rooftop::MIN_X, Rooftop::MIN_Y },
                           { Rooftop::MIN_X + Rooftop::WIDTH, Rooftop::MIN_Y + Rooftop::HEIGHT });
        m_camera.SetPosition(player.GetPosition());
        Logger::Instance().Log(Logger::Severity::Event, "Checkpoint respawn: Rooftop");
        break;
    }
    case MapZone::Underground:
    {
        player.SetSizeScale(1.0f);
        float playerStartX = Underground::MIN_X + 100.0f;
        float playerStartY = Underground::MIN_Y + 270.0f;
        player.SetCurrentGroundLevel(Underground::MIN_Y + 75.0f);
        player.SetPosition({ playerStartX, playerStartY });
        player.SetOnGround(false);
        m_camera.SetBounds({ Underground::MIN_X, Underground::MIN_Y },
                           { Underground::MIN_X + Underground::WIDTH, Underground::MIN_Y + Underground::HEIGHT });
        m_camera.SetPosition(player.GetPosition());
        if (m_underground)
            m_underground->RefillPulseSourcesAfterCheckpointRespawn();
        Logger::Instance().Log(Logger::Severity::Event, "Checkpoint respawn: Underground");
        break;
    }
    case MapZone::Train:
    {
        // Keep zone flags consistent after returning from GameOver stack.
        m_rooftopAccessed = true;
        m_undergroundAccessed = true;
        m_trainAccessed = true;

        player.SetSizeScale(0.6f);
        float playerStartX = Train::MIN_X + 300.0f;
        float playerStartY = Train::MIN_Y + 540.0f;
        player.SetCurrentGroundLevel(Train::MIN_Y + 90.0f);
        player.SetPosition({ playerStartX, playerStartY });
        player.SetOnGround(false);
        m_camera.SetBounds({ Train::MIN_X, Train::MIN_Y },
                           { Train::MIN_X + m_train->GetMapWidth(), Train::MIN_Y + Train::HEIGHT });
        // Force immediate camera snap to player so respawn frame never renders off-screen.
        m_camera.Update(player.GetPosition(), 1.0f);
        if (m_train)
            m_train->RestartEntryTimer(); // reset offset/speed/countdown/sounds to first-entry state
        Logger::Instance().Log(Logger::Severity::Event, "Checkpoint respawn: Train");
        break;
    }
    }
}

Math::Vec2 GameplayState::ScreenToWorldCoordinates(double screenX, double screenY) const
{
    Engine& engine = gsm.GetEngine();
    int viewportX = 0;
    int viewportY = 0;
    int viewportWidth = 0;
    int viewportHeight = 0;
    engine.GetPostProcess().GetLetterboxViewport(viewportX, viewportY, viewportWidth, viewportHeight);
    if (viewportWidth <= 0 || viewportHeight <= 0)
    {
        glfwGetFramebufferSize(engine.GetWindow(), &viewportWidth, &viewportHeight);
        viewportX = 0;
        viewportY = 0;
    }
    if (viewportWidth <= 0 || viewportHeight <= 0)
        return m_camera.GetPosition();

    float mouseViewportX = static_cast<float>(screenX - viewportX);
    float mouseViewportY = static_cast<float>(screenY - viewportY);
    mouseViewportX = std::clamp(mouseViewportX, 0.0f, static_cast<float>(viewportWidth));
    mouseViewportY = std::clamp(mouseViewportY, 0.0f, static_cast<float>(viewportHeight));

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

void GameplayState::WorldToFramebuffer(Math::Vec2 world, double& outFbX, double& outFbY) const
{
    Math::Vec2 cam = m_camera.GetPosition();
    const float mouseGameX = world.x - cam.x + GAME_WIDTH * 0.5f;
    const float mouseGameY = world.y - cam.y + GAME_HEIGHT * 0.5f;
    const float mouseNDCX = mouseGameX / GAME_WIDTH;
    const float mouseNDCY = 1.0f - mouseGameY / GAME_HEIGHT;

    Engine& engine = gsm.GetEngine();
    int vx = 0;
    int vy = 0;
    int vw = 0;
    int vh = 0;
    engine.GetPostProcess().GetLetterboxViewport(vx, vy, vw, vh);
    if (vw <= 0 || vh <= 0)
    {
        glfwGetFramebufferSize(engine.GetWindow(), &vw, &vh);
        vx = 0;
        vy = 0;
    }
    if (vw <= 0 || vh <= 0)
    {
        outFbX = 0.0;
        outFbY = 0.0;
        return;
    }

    const float mvx = mouseNDCX * static_cast<float>(vw);
    const float mvy = mouseNDCY * static_cast<float>(vh);
    outFbX = static_cast<double>(vx) + static_cast<double>(mvx);
    outFbY = static_cast<double>(vy) + static_cast<double>(mvy);
}

void GameplayState::ApplyGamepadDroneTargetingAssist(double dt, Input::Input& input, Math::Vec2& inOutMouseWorldPos)
{
    const Math::Vec2 cam = m_camera.GetPosition();
    const float curGameX = inOutMouseWorldPos.x - cam.x + GAME_WIDTH * 0.5f;
    const float curGameY = inOutMouseWorldPos.y - cam.y + GAME_HEIGHT * 0.5f;

    const Drone* nearestMagnet = nullptr;
    float bestMagnetDistSq = 1.0e30f;

    auto considerMagnet = [&](const Drone& d) {
        if (d.IsDead() || d.IsHit())
            return;
        const Math::Vec2 c = DronePulseTargetCenter(d);
        const float gX = c.x - cam.x + GAME_WIDTH * 0.5f;
        const float gY = c.y - cam.y + GAME_HEIGHT * 0.5f;
        const float dx = gX - curGameX;
        const float dy = gY - curGameY;
        const float dsq = dx * dx + dy * dy;
        if (dsq < bestMagnetDistSq)
        {
            bestMagnetDistSq = dsq;
            nearestMagnet = &d;
        }
    };

    for (const auto& d : droneManager->GetDrones())
        considerMagnet(d);
    for (const auto& d : m_hallway->GetDrones())
        considerMagnet(d);
    for (const auto& d : m_rooftop->GetDrones())
        considerMagnet(d);
    if (m_undergroundAccessed)
    {
        for (const auto& d : m_underground->GetDrones())
            considerMagnet(d);
    }
    if (m_trainAccessed)
    {
        for (const auto& d : m_train->GetDrones())
            considerMagnet(d);
        if (m_train && m_train->GetCarTransportDroneManager())
        {
            for (const auto& d : m_train->GetCarTransportDroneManager()->GetDrones())
                considerMagnet(d);
        }
        if (m_train && m_train->GetSirenDroneManager())
        {
            for (const auto& d : m_train->GetSirenDroneManager()->GetDrones())
                considerMagnet(d);
        }
    }

    // R3: snap cursor to nearest drone within world distance (LB is pulse absorb).
    if (input.IsGamepadButtonTriggered(GLFW_GAMEPAD_BUTTON_RIGHT_THUMB))
    {
        const Drone* best = nullptr;
        float bestWorldSq = 1.0e30f;
        const float maxSq = kPadManualSnapMaxWorld * kPadManualSnapMaxWorld;

        auto considerManual = [&](const Drone& d) {
            if (d.IsDead() || d.IsHit())
                return;
            const Math::Vec2 c = DronePulseTargetCenter(d);
            const float dx = inOutMouseWorldPos.x - c.x;
            const float dy = inOutMouseWorldPos.y - c.y;
            const float dsq = dx * dx + dy * dy;
            if (dsq <= maxSq && dsq < bestWorldSq)
            {
                bestWorldSq = dsq;
                best = &d;
            }
        };

        for (const auto& d : droneManager->GetDrones())
            considerManual(d);
        for (const auto& d : m_hallway->GetDrones())
            considerManual(d);
        for (const auto& d : m_rooftop->GetDrones())
            considerManual(d);
        if (m_undergroundAccessed)
        {
            for (const auto& d : m_underground->GetDrones())
                considerManual(d);
        }
        if (m_trainAccessed)
        {
            for (const auto& d : m_train->GetDrones())
                considerManual(d);
            if (m_train && m_train->GetCarTransportDroneManager())
            {
                for (const auto& d : m_train->GetCarTransportDroneManager()->GetDrones())
                    considerManual(d);
            }
            if (m_train && m_train->GetSirenDroneManager())
            {
                for (const auto& d : m_train->GetSirenDroneManager()->GetDrones())
                    considerManual(d);
            }
        }

        if (best)
        {
            const Math::Vec2 targetW = DronePulseTargetCenter(*best);
            double fbX = 0.0;
            double fbY = 0.0;
            WorldToFramebuffer(targetW, fbX, fbY);
            input.SetMouseFramebufferPosition(fbX, fbY);
            input.ClearGamepadAimVelocity();
            inOutMouseWorldPos = targetW;
            return;
        }
    }

    if (!nearestMagnet)
        return;

    const float dist = std::sqrt(bestMagnetDistSq);
    const Math::Vec2 targetW = DronePulseTargetCenter(*nearestMagnet);
    double curFbX = 0.0;
    double curFbY = 0.0;
    input.GetMousePosition(curFbX, curFbY);
    double tgtFbX = 0.0;
    double tgtFbY = 0.0;
    WorldToFramebuffer(targetW, tgtFbX, tgtFbY);

    if (dist < kPadMagnetInnerGame)
    {
        input.SetMouseFramebufferPosition(tgtFbX, tgtFbY);
        input.ClearGamepadAimVelocity();
        inOutMouseWorldPos = targetW;
    }
    else if (dist < kPadMagnetOuterGame)
    {
        const float span = (std::max)(1.0e-3f, kPadMagnetOuterGame - kPadMagnetInnerGame);
        const float u = (kPadMagnetOuterGame - dist) / span;
        const float alpha = (std::min)(1.0f, u * u * (22.0f * static_cast<float>(dt)));
        const double nx = curFbX + (tgtFbX - curFbX) * static_cast<double>(alpha);
        const double ny = curFbY + (tgtFbY - curFbY) * static_cast<double>(alpha);
        input.SetMouseFramebufferPosition(nx, ny);
        inOutMouseWorldPos = ScreenToWorldCoordinates(nx, ny);
    }
}

void GameplayState::Draw()
{
    DrawMainLayer();
    DrawForegroundLayer(false);
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
    else if (playerPos.y <= Train::MIN_Y + Train::HEIGHT)
    {
        // Sunset sky base colour (deep dark blue at very top – gradient drawn by DrawBackground)
        r = 7.0f / 255.0f; g = 5.0f / 255.0f; b = 18.0f / 255.0f;
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
    // Do NOT call glfwGetFramebufferSize here — on Retina displays the physical framebuffer is larger
    // than the FBO, which would cause only the bottom-left portion to be rendered.
    GL::ClearColor(r, g, b, 1.0f);
    GL::Clear(GL_COLOR_BUFFER_BIT);

    GL::Enable(GL_BLEND);
    GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Shader& textureShader = engine.GetTextureShader();

    // Zoom-aware world projection: effectiveWidth/Height shrink as zoom increases (zoom-in effect)
    const float effectiveWidth  = GAME_WIDTH  / m_cameraZoom;
    const float effectiveHeight = GAME_HEIGHT / m_cameraZoom;
    const float viewHalfW = effectiveWidth * 0.5f;
    Math::Matrix worldProjection;
    {
        Math::Vec2 camPos   = m_camera.GetPosition();
        Math::Vec2 camShake = m_camera.GetScreenShakeOffset();
        float      offsetX  = std::round(effectiveWidth * 0.5f - camPos.x + camShake.x);
        float      offsetY  = std::round(effectiveHeight * 0.5f - camPos.y + camShake.y);
        Math::Matrix zoomedOrtho = Math::Matrix::CreateOrtho(
            0.0f, effectiveWidth, 0.0f, effectiveHeight, -1.0f, 1.0f);
        Math::Matrix zoomedView = Math::Matrix::CreateTranslation({ offsetX, offsetY });
        worldProjection = zoomedOrtho * zoomedView;

        textureShader.use();
        textureShader.setMat4("projection", worldProjection);
    }
    textureShader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
    textureShader.setBool("flipX", false);

    // 1a) Train sunset sky gradient (drawn before everything else so it sits behind all sprites)
    if (m_trainAccessed)
    {
        colorShader->use();
        colorShader->setMat4("projection", worldProjection);
        m_train->DrawBackground(*colorShader, m_camera.GetPosition(), viewHalfW);
        // Restore texture shader state
        textureShader.use();
        textureShader.setMat4("projection", worldProjection);
        textureShader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
        textureShader.setBool("flipX", false);
        // 레일은 Rail.png가 장면 최하단 레이어이므로 하늘 바로 다음·다른 맵·기차 본체보다 먼저 그린다.
        m_train->DrawRailTrack(textureShader, m_camera.GetPosition(), viewHalfW);
    }

    // 1b) World maps (post-processed: exposure / hallway overlay)
    m_room->Draw(textureShader);
    m_hallway->Draw(textureShader);
    m_rooftop->Draw(textureShader);
    m_underground->Draw(textureShader);
    m_train->Draw(textureShader, m_camera.GetPosition(), viewHalfW);
    if (m_trainAccessed)
    {
        colorShader->use();
        colorShader->setMat4("projection", worldProjection);
        colorShader->setFloat("uAlpha", 1.0f);
        m_train->DrawRobotTrainAlerts(*colorShader, *m_debugRenderer);
        textureShader.use();
        textureShader.setMat4("projection", worldProjection);
        textureShader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
        textureShader.setBool("flipX", false);
        m_train->DrawCar2EnterLeavePrompt(textureShader, m_camera.GetPosition(), viewHalfW);
    }

    if (m_trainAccessed)
    {
        textureShader.use();
        textureShader.setMat4("projection", worldProjection);
        textureShader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
        textureShader.setBool("flipX", false);
        textureShader.setFloat("alpha", 1.0f);
        m_train->DrawCarTransportOverlays(textureShader, m_camera.GetPosition(), viewHalfW);
    }

    if (m_trainAccessed)
    {
        colorShader->use();
        colorShader->setMat4("projection", worldProjection);
        GL::Enable(GL_BLEND);
        GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        m_train->DrawCar3SirenWaves(*colorShader, m_camera.GetPosition(), viewHalfW);
        m_train->DrawCarTransportVFX(*colorShader, m_camera.GetPosition(), viewHalfW);
        m_train->DrawValveWaterVFX(*colorShader, worldProjection, m_camera.GetPosition(), viewHalfW);
        textureShader.use();
        textureShader.setMat4("projection", worldProjection);
        textureShader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
        textureShader.setBool("flipX", false);
    }

    // Hallway railings: DrawForegroundLayer (after player / VFX) so Railing.png sits in front.

    GL::Disable(GL_BLEND);
}

void GameplayState::DrawForegroundLayer(bool compositeToScreen)
{
    if (m_isGameOver)
        return;

    Engine& engine = gsm.GetEngine();

    if (compositeToScreen)
    {
        GL::BindFramebuffer(GL_FRAMEBUFFER, 0);
        int vpX = 0, vpY = 0, vpW = 0, vpH = 0;
        engine.GetPostProcess().GetLetterboxViewport(vpX, vpY, vpW, vpH);
        GL::Viewport(vpX, vpY, vpW, vpH);
    }
    else
    {
        GL::Viewport(0, 0, static_cast<int>(GAME_WIDTH), static_cast<int>(GAME_HEIGHT));
    }

    GL::Disable(GL_DEPTH_TEST);

    // Screen-space projection (for HUD, fonts, pulse gauge — not zoom-affected)
    Math::Matrix baseProjection = Math::Matrix::CreateOrtho(
        0.0f, GAME_WIDTH,
        0.0f, GAME_HEIGHT,
        -1.0f, 1.0f
    );
    // Zoom-aware world projection
    const float fgEffectiveWidth  = GAME_WIDTH  / m_cameraZoom;
    const float fgEffectiveHeight = GAME_HEIGHT / m_cameraZoom;
    Math::Vec2 fgCamPos = m_camera.GetPosition();
    float fgOffsetX = std::round(fgEffectiveWidth  * 0.5f - fgCamPos.x);
    float fgOffsetY = std::round(fgEffectiveHeight * 0.5f - fgCamPos.y);
    Math::Matrix fgZoomedOrtho = Math::Matrix::CreateOrtho(
        0.0f, fgEffectiveWidth, 0.0f, fgEffectiveHeight, -1.0f, 1.0f);
    Math::Matrix fgZoomedView = Math::Matrix::CreateTranslation({ fgOffsetX, fgOffsetY });
    Math::Matrix projection = fgZoomedOrtho * fgZoomedView;

    GL::Enable(GL_BLEND);
    GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Shader& textureShader = engine.GetTextureShader();

    // 4) Sprite outlines (world-space)
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
            {
                robot.DrawOutline(*m_outlineShader);
            }
        }
        for (const auto& robot : m_train->GetRobots())
        {
            if (!robot.IsDead())
            {
                robot.DrawOutline(*m_outlineShader);
            }
        }
    }
    GL::Disable(GL_BLEND);

    GL::Enable(GL_BLEND);
    GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    textureShader.use();
    textureShader.setMat4("projection", projection);
    textureShader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
    textureShader.setBool("flipX", false);

    // Drones (same order as former main pass: per-map managers, then room tracers)
    m_hallway->DrawDrones(textureShader);
    m_rooftop->DrawDrones(textureShader);
    m_underground->DrawDrones(textureShader);
    m_train->DrawDrones(textureShader);
    droneManager->Draw(textureShader);

    // Pulse charger "remain" bars: draw before the player so the gauge sits behind the character.
    colorShader->use();
    colorShader->setMat4("projection", projection);
    colorShader->setFloat("uAlpha", 0.72f);
    for (const auto& src : m_room->GetPulseSources())
        src.DrawRemainGauge(*colorShader);
    for (const auto& src : m_hallway->GetPulseSources())
        src.DrawRemainGauge(*colorShader);
    for (const auto& src : m_rooftop->GetPulseSources())
        src.DrawRemainGauge(*colorShader);
    for (const auto& src : m_underground->GetPulseSources())
        src.DrawRemainGauge(*colorShader);
    for (const auto& src : m_train->GetPulseSources())
        src.DrawRemainGauge(*colorShader);
    if (m_trainAccessed && m_train)
    {
        m_train->DrawCar3SirenProgressGauge(*colorShader, fgCamPos, fgEffectiveWidth * 0.5f);
        m_train->DrawCarTransportInjectProgressGauge(*colorShader, fgCamPos, fgEffectiveWidth * 0.5f);
        m_train->DrawCar2InsideLockTimer(*colorShader, fgCamPos, fgEffectiveWidth * 0.5f);
    }
    colorShader->setFloat("uAlpha", 1.0f);

    textureShader.use();
    textureShader.setMat4("projection", projection);
    textureShader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
    textureShader.setBool("flipX", false);
    player.Draw(textureShader);

    textureShader.use();
    textureShader.setMat4("projection", projection);
    textureShader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
    textureShader.setBool("flipX", false);
    pulseManager->DrawVFX(textureShader);

    // DrawVFX ends with GL_BLEND disabled; re-enable so railing alpha blends correctly
    // (otherwise transparent texels overwrite the scene with black during attacks).
    GL::Enable(GL_BLEND);
    GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Hallway railings on top of player (and drones / pulse VFX in overlap)
    textureShader.use();
    textureShader.setMat4("projection", projection);
    textureShader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
    textureShader.setBool("flipX", false);
    textureShader.setFloat("alpha", 1.0f);
    textureShader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
    textureShader.setFloat("tintStrength", 0.0f);
    m_hallway->DrawForeground(textureShader);

    // Hiding-box S.png: hall darkening active + player within range of spot top-center
    if (m_doorOpened && !m_rooftopAccessed && m_hallwayHidingPromptS && m_hallwayHidingPromptS->GetTextureID() != 0)
    {
        const Math::Vec2 playerPos = player.GetPosition();
        for (const auto& spot : m_hallway->GetHidingSpots())
        {
            const Math::Vec2 topCenter = {
                spot.pos.x,
                spot.pos.y + spot.size.y * 0.5f + HIDING_S_ICON_OFFSET_Y
            };
            if ((playerPos - topCenter).LengthSq() > HIDING_S_PROMPT_DISTANCE_SQ)
                continue;

            textureShader.use();
            textureShader.setMat4("projection", projection);
            textureShader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
            textureShader.setBool("flipX", false);
            textureShader.setFloat("alpha", 1.0f);
            textureShader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
            textureShader.setFloat("tintStrength", 0.0f);

            Math::Matrix sModel = Math::Matrix::CreateTranslation(topCenter)
                * Math::Matrix::CreateScale({ HIDING_S_ICON_WORLD_SIZE, HIDING_S_ICON_WORLD_SIZE });
            m_hallwayHidingPromptS->Draw(textureShader, sModel);
        }
    }

    // 6) World-space overlays (radars / gauges)
    colorShader->use();
    colorShader->setMat4("projection", projection);
    droneManager->DrawRadars(*colorShader, *m_debugRenderer);
    m_hallway->DrawRadars(*colorShader, *m_debugRenderer);
    m_rooftop->DrawRadars(*colorShader, *m_debugRenderer);
    m_underground->DrawRadars(*colorShader, *m_debugRenderer);
    m_train->DrawRadars(*colorShader, *m_debugRenderer);

    droneManager->DrawGauges(*colorShader, *m_debugRenderer);
    m_hallway->DrawGauges(*colorShader, *m_debugRenderer);
    m_rooftop->DrawGauges(*colorShader, *m_debugRenderer);
    m_underground->DrawGauges(*colorShader, *m_debugRenderer);
    m_train->DrawGauges(*colorShader, *m_debugRenderer);

    pulseManager->DrawDetonationVFX(*colorShader, *m_debugRenderer);

    // 7) Fullscreen frame overlay (1920x1080), camera-locked in world space
    if (m_hudFrame && m_hudFrame->GetWidth() > 0)
    {
        GL::Enable(GL_BLEND);
        GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        textureShader.use();
        textureShader.setMat4("projection", projection);
        textureShader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
        textureShader.setBool("flipX", false);
        textureShader.setFloat("alpha", 1.0f);
        textureShader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
        textureShader.setFloat("tintStrength", 0.0f);

        Math::Vec2 camCenter = m_camera.GetPosition();
        // Scale HUD frame to effective view size so it always fills the screen regardless of zoom
        Math::Matrix hudModel = Math::Matrix::CreateTranslation(camCenter)
            * Math::Matrix::CreateScale({ fgEffectiveWidth, fgEffectiveHeight });
        m_hudFrame->Draw(textureShader, hudModel);

        GL::Disable(GL_BLEND);
    }

    // 8) Screen-space HUD (pulse gauge)
    GL::Enable(GL_BLEND);
    GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    textureShader.use();
    textureShader.setMat4("projection", baseProjection);
    textureShader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
    textureShader.setBool("flipX", false);
    textureShader.setFloat("alpha", 1.0f);
    textureShader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
    textureShader.setFloat("tintStrength", 0.0f);

    m_pulseGauge.Draw(textureShader);

    // 9) Fonts / minimap / tutorial
    m_fontShader->use();
    m_fontShader->setMat4("projection", baseProjection);

    m_font->DrawBakedText(*m_fontShader, m_fpsText, { 20.f, GAME_HEIGHT - 40.f }, 32.0f);

    m_pulseDetonateSkill.DrawCooldownUI(*m_font, *m_fontShader, GAME_HEIGHT - 80.f);

    std::string countdownText = m_rooftop->GetLiftCountdownText();
    if (!countdownText.empty())
    {
        CachedTextureInfo countdownTexture = m_font->PrintToTexture(*m_fontShader, countdownText);
        m_font->DrawBakedText(*m_fontShader, countdownTexture, { GAME_WIDTH / 2.0f - 250.0f, 100.0f }, 50.0f);
    }

    // Train departure countdown / status — Conversion.png style, but non-blocking.
    if (m_trainAccessed && m_train)
    {
        std::string trainMsg = m_train->GetDepartureAnnouncementText();
        if (!trainMsg.empty())
        {
            constexpr float kTrainBoxW = 1680.0f;
            constexpr float kTrainBoxH = 240.0f;
            constexpr float kTrainBoxBottomMargin = 36.0f;
            constexpr float kTrainBannerFontH = 40.0f;
            const float boxCenterX = GAME_WIDTH * 0.5f;
            const float boxCenterY = kTrainBoxBottomMargin + kTrainBoxH * 0.5f;

            if (m_conversionBackdrop && m_conversionBackdrop->GetWidth() > 0)
            {
                textureShader.use();
                textureShader.setMat4("projection", baseProjection);
                textureShader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
                textureShader.setBool("flipX", false);
                textureShader.setFloat("alpha", 1.0f);
                textureShader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
                textureShader.setFloat("tintStrength", 0.0f);
                Math::Matrix trainBoxModel = Math::Matrix::CreateTranslation({ boxCenterX, boxCenterY })
                    * Math::Matrix::CreateScale({ kTrainBoxW, kTrainBoxH });
                m_conversionBackdrop->Draw(textureShader, trainBoxModel);
            }

            m_fontShader->use();
            m_fontShader->setMat4("projection", baseProjection);
            CachedTextureInfo trainMsgTex = m_font->PrintToTexture(*m_fontShader, trainMsg);
            const float rw =
                static_cast<float>(trainMsgTex.width) * (kTrainBannerFontH / static_cast<float>(m_font->m_fontHeight));
            m_font->DrawBakedText(*m_fontShader, trainMsgTex,
                { boxCenterX - rw * 0.5f, boxCenterY - kTrainBannerFontH * 0.35f }, kTrainBannerFontH);
        }

        std::string valveHint = m_train->GetCar5ValveHintBannerText();
        if (!valveHint.empty())
        {
            constexpr float kValveHintFontH = 38.0f;
            CachedTextureInfo valveTex = m_font->PrintToTexture(*m_fontShader, valveHint);
            const float rw =
                static_cast<float>(valveTex.width) * (kValveHintFontH / static_cast<float>(m_font->m_fontHeight));
            m_font->DrawBakedText(*m_fontShader, valveTex, { GAME_WIDTH * 0.5f - rw * 0.5f, 125.0f },
                                  kValveHintFontH);
        }
    }

    m_tutorial->Draw(*m_font, *m_fontShader);

    if (m_storyDialogue && m_storyDialogue->IsBlocking())
    {
        Shader& texForStory = engine.GetTextureShader();
        m_storyDialogue->Draw(*m_font, texForStory, *m_fontShader, baseProjection);

        // Click hint (matches StoryDialogue box size/margin); keyed cursor texture, no black matte.
        constexpr float STORY_BOX_WIDTH = 1680.0f;
        constexpr float STORY_BOX_BOTTOM_MARGIN = 36.0f;
        if (m_mouseLeftCursor && m_mouseLeftCursor->GetWidth() > 0)
        {
            GL::Enable(GL_BLEND);
            GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            Shader& ts = engine.GetTextureShader();
            ts.use();
            ts.setMat4("projection", baseProjection);
            ts.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
            ts.setBool("flipX", false);
            ts.setFloat("alpha", 1.0f);
            ts.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
            ts.setFloat("tintStrength", 0.0f);
            const float boxCx = GAME_WIDTH * 0.5f;
            const Math::Vec2 hintPos = { boxCx + STORY_BOX_WIDTH * 0.5f - 72.0f, STORY_BOX_BOTTOM_MARGIN + 50.0f };
            constexpr float kHintIconSize = 28.0f;
            Math::Matrix hintModel =
                Math::Matrix::CreateTranslation(hintPos) * Math::Matrix::CreateScale({ kHintIconSize, kHintIconSize });
            m_mouseLeftCursor->Draw(ts, hintModel);
        }
    }

    GL::Enable(GL_BLEND);

    // 10) Cursor
    Engine& engineForCursor = gsm.GetEngine();
    auto& inputForCursor = engineForCursor.GetInput();
    double mouseScreenX, mouseScreenY;
    inputForCursor.GetMousePosition(mouseScreenX, mouseScreenY);

    int viewportXForCursor = 0;
    int viewportYForCursor = 0;
    int viewportWidthForCursor = 0;
    int viewportHeightForCursor = 0;
    engineForCursor.GetPostProcess().GetLetterboxViewport(
        viewportXForCursor, viewportYForCursor, viewportWidthForCursor, viewportHeightForCursor);
    if (viewportWidthForCursor <= 0 || viewportHeightForCursor <= 0)
    {
        glfwGetFramebufferSize(
            engineForCursor.GetWindow(), &viewportWidthForCursor, &viewportHeightForCursor);
        viewportXForCursor = 0;
        viewportYForCursor = 0;
    }
    if (viewportWidthForCursor <= 0 || viewportHeightForCursor <= 0)
        return;

    float mouseViewportX = static_cast<float>(mouseScreenX - viewportXForCursor);
    float mouseViewportY = static_cast<float>(mouseScreenY - viewportYForCursor);
    mouseViewportX = std::clamp(mouseViewportX, 0.0f, static_cast<float>(viewportWidthForCursor));
    mouseViewportY = std::clamp(mouseViewportY, 0.0f, static_cast<float>(viewportHeightForCursor));

    float mouseNDCX = mouseViewportX / static_cast<float>(viewportWidthForCursor);
    float mouseNDCY = mouseViewportY / static_cast<float>(viewportHeightForCursor);

    float mouseGameX = mouseNDCX * GAME_WIDTH;
    float mouseGameY = (1.0f - mouseNDCY) * GAME_HEIGHT;

    colorShader->use();
    colorShader->setMat4("projection", baseProjection);

    Math::Vec2 cursorPos = { mouseGameX, mouseGameY };

    // Cursor sprite mode:
    // - Hover actionable left target => MouseLeft.png
    // - Hover pulse charger (right) => MouseRight.png
    // - Hover combat unit out of attack range / blocked => MouseIdle.png
    // - Hover other world geometry without usable interaction => MouseIdle.png
    // - Empty space / no target => MousePointer.png
    bool overLeftClickTarget     = false;
    bool overRightClickTarget    = false;
    bool showCombatIdleCursor    = false;
    bool showIdleOverWorldObject = false;

    const bool storyDialogueBlocking = m_storyDialogue && m_storyDialogue->IsBlocking();

    if (!m_isDebugDraw && !storyDialogueBlocking)
    {
        const Math::Vec2 mouseWorldPosForHover = m_lastMouseWorldPos;
        const Math::Vec2 playerHitboxCenter    = player.GetHitboxCenter();
        const Math::Vec2 playerHitboxSize      = player.GetHitboxSize();
        const Math::Vec2 playerPosForCursor    = player.GetPosition();

        // Left-click targets
        if (m_door && !m_door->IsOpened() && m_door->IsPlayerNearby() &&
            Collision::CheckPointInAABB(mouseWorldPosForHover, m_door->GetPosition(), m_door->GetSize()))
            overLeftClickTarget = true;

        if (!overLeftClickTarget && m_rooftopDoor && !m_rooftopDoor->IsOpened() && m_rooftopDoor->IsPlayerNearby() &&
            Collision::CheckPointInAABB(mouseWorldPosForHover, m_rooftopDoor->GetPosition(), m_rooftopDoor->GetSize()))
            overLeftClickTarget = true;

        if (!overLeftClickTarget && m_room && m_room->IsPlayerInBlindArea() && !m_room->IsBlindOpen() &&
            Collision::CheckPointInAABB(mouseWorldPosForHover, m_room->GetBlindPos(), m_room->GetBlindSize()))
            overLeftClickTarget = true;

        if (!overLeftClickTarget && m_rooftop && m_rooftop->IsPlayerCloseToHole() && !m_rooftop->IsHoleClosed() &&
            Collision::CheckPointInAABB(mouseWorldPosForHover, m_rooftop->GetHolePos(), m_rooftop->GetHoleSize()))
            overLeftClickTarget = true;

        if (!overLeftClickTarget && m_rooftop && m_rooftop->IsPlayerNearLift() &&
            Collision::CheckPointInAABB(mouseWorldPosForHover, m_rooftop->GetLiftButtonPos(), m_rooftop->GetLiftButtonSize()))
            overLeftClickTarget = true;

        if (!overLeftClickTarget && m_trainAccessed && m_train && m_train->GetCar3SirenActive() &&
            m_train->IsCar3SirenMouseHoverForPulseInject(playerHitboxCenter, playerHitboxSize, mouseWorldPosForHover))
            overLeftClickTarget = true;

        // Right-click targets (pulse chargers): player overlaps AND cursor is inside the source
        auto checkPulseSources = [&](const std::vector<PulseSource>& sources) {
            for (const auto& src : sources)
            {
                if (!Collision::CheckAABB(playerHitboxCenter, playerHitboxSize, src.GetPosition(), src.GetHitboxSize()))
                    continue;
                if (Collision::CheckPointInAABB(mouseWorldPosForHover, src.GetPosition(), src.GetHitboxSize()))
                    return true;
            }
            return false;
        };

        if (checkPulseSources(m_room->GetPulseSources()) ||
            checkPulseSources(m_hallway->GetPulseSources()) ||
            checkPulseSources(m_rooftop->GetPulseSources()) ||
            checkPulseSources(m_underground->GetPulseSources()) ||
            checkPulseSources(m_train->GetPulseSources()))
        {
            overRightClickTarget = true;
        }

        // Combat: beam과 동일 — 사거리 안 + 입력 가능할 때만 좌클릭 커서; 그 외 호버는 Idle
        bool combatHoverInRange    = false;
        bool combatHoverOutOfRange = false;

        const bool hallwayHidingBlocksDroneAttack =
            m_doorOpened && !m_rooftopAccessed
            && m_hallway->IsPlayerHiding(playerPosForCursor, playerHitboxSize, player.IsCrouching());
        const bool hideRoom =
            m_room->IsPlayerHiding(playerPosForCursor, playerHitboxSize, player.IsCrouching());
        const bool hideHall =
            m_hallway->IsPlayerHiding(playerPosForCursor, playerHitboxSize, player.IsCrouching());
        const bool hideUg =
            m_undergroundAccessed && m_underground
            && m_underground->IsPlayerHiding(playerPosForCursor, playerHitboxSize, player.IsCrouching());
        const bool hideTrain =
            m_trainAccessed && m_train
            && m_train->IsPlayerHiding(playerHitboxCenter, playerHitboxSize, player.IsCrouching());
        const bool inCar2PulseBoxForCursor =
            m_trainAccessed && m_train
            && m_train->IsPlayerInCar2PurplePulseBox(playerHitboxCenter, playerHitboxSize);
        const bool cursorCombatAttackAllowed =
            !(player.IsCrouching() && (hideRoom || hideHall || hideUg || hideTrain)) && !inCar2PulseBoxForCursor;

        const float droneHoverScale =
            inputForCursor.IsGamepadConnected() ? kGamepadDroneAimHitboxScale : 0.8f;
        const Math::Vec2 kCursorHitboxCombat{ 32.0f, 32.0f };
        auto isMouseOnDroneForCursor = [&](const Drone& drone) {
            if (drone.IsDead() || drone.IsHit()) return false;
            const Math::Vec2 hb = drone.GetSize() * droneHoverScale;
            return Collision::CheckPointInAABB(mouseWorldPosForHover, drone.GetPosition(), hb)
                || Collision::CheckAABB(mouseWorldPosForHover, kCursorHitboxCombat, drone.GetPosition(), hb);
        };
        auto isMouseOnRobotForCursor = [&](const Robot& robot) {
            if (robot.IsDead()) return false;
            const Math::Vec2 sz = robot.GetSize();
            return Collision::CheckPointInAABB(mouseWorldPosForHover, robot.GetPosition(), sz)
                || Collision::CheckAABB(mouseWorldPosForHover, kCursorHitboxCombat, robot.GetPosition(), sz);
        };

        auto droneInAttackRangeForCursor = [&](const Drone& drone) {
            const float      distSq = (playerHitboxCenter - drone.GetPosition()).LengthSq();
            const Math::Vec2 hb     = drone.GetSize() * droneHoverScale;
            const float      rad    = (hb.x + hb.y) * 0.25f;
            const float      limitSq = (ATTACK_RANGE + rad) * (ATTACK_RANGE + rad);
            return distSq < limitSq;
        };
        auto robotInAttackRangeForCursor = [&](const Robot& robot) {
            const float      distSq = (playerHitboxCenter - robot.GetPosition()).LengthSq();
            const Math::Vec2 sz     = robot.GetSize();
            const float      rad    = (sz.x + sz.y) * 0.25f;
            const float      limitSq = (ATTACK_RANGE + rad) * (ATTACK_RANGE + rad);
            return distSq < limitSq;
        };
        auto considerDroneCombatCursor = [&](const Drone& drone, bool groupAllowsAttack) {
            if (drone.IsDead() || drone.IsHit()) return;
            if (!isMouseOnDroneForCursor(drone)) return;
            const bool ok =
                cursorCombatAttackAllowed && groupAllowsAttack && droneInAttackRangeForCursor(drone);
            if (ok) combatHoverInRange = true;
            else combatHoverOutOfRange = true;
        };
        auto considerRobotCombatCursor = [&](const Robot& robot, bool groupAllowsAttack) {
            if (robot.IsDead()) return;
            if (!isMouseOnRobotForCursor(robot)) return;
            const bool ok =
                cursorCombatAttackAllowed && groupAllowsAttack && robotInAttackRangeForCursor(robot);
            if (ok) combatHoverInRange = true;
            else combatHoverOutOfRange = true;
        };

        for (const auto& d : droneManager->GetDrones())
            considerDroneCombatCursor(d, true);
        for (const auto& d : m_hallway->GetDrones())
            considerDroneCombatCursor(d, !hallwayHidingBlocksDroneAttack);
        for (const auto& d : m_rooftop->GetDrones()) considerDroneCombatCursor(d, true);
        if (m_undergroundAccessed)
        {
            for (const auto& d : m_underground->GetDrones()) considerDroneCombatCursor(d, true);
            for (const auto& r : m_underground->GetRobots()) considerRobotCombatCursor(r, true);
        }
        if (m_trainAccessed && m_train)
        {
            for (const auto& d : m_train->GetDrones()) considerDroneCombatCursor(d, true);
            if (m_train->GetCarTransportDroneManager())
            {
                for (const auto& d : m_train->GetCarTransportDroneManager()->GetDrones())
                    considerDroneCombatCursor(d, true);
            }
            if (m_train->GetSirenDroneManager())
            {
                for (const auto& d : m_train->GetSirenDroneManager()->GetDrones())
                    considerDroneCombatCursor(d, true);
            }
            for (const auto& r : m_train->GetRobots()) considerRobotCombatCursor(r, true);
        }

        if (combatHoverInRange) overLeftClickTarget = true;

        if (!overLeftClickTarget && m_trainAccessed && m_train &&
            m_train->IsCar2EnterLeavePromptHovered(playerHitboxCenter, playerHitboxSize, mouseWorldPosForHover, fgCamPos,
                                                   fgEffectiveWidth * 0.5f))
            overLeftClickTarget = true;

        // 운반 차량: 플레이어와 차 히트박스 겹침 + 마우스가 차 위 → 좌클릭 커서
        if (!overLeftClickTarget && m_trainAccessed && m_train)
        {
            int carSlot = -1;
            if (m_train->TryGetCarTransportClickIgniteTarget(playerHitboxCenter, playerHitboxSize,
                                                             mouseWorldPosForHover, carSlot))
                overLeftClickTarget = true;
        }

        if (!overLeftClickTarget && m_trainAccessed && m_train)
        {
            if (m_train->IsValveMouseHoverable(playerHitboxCenter, playerHitboxSize, mouseWorldPosForHover))
                overLeftClickTarget = true;
        }

        showCombatIdleCursor =
            combatHoverOutOfRange && !combatHoverInRange && !overLeftClickTarget && !overRightClickTarget;

        if (!overLeftClickTarget && !overRightClickTarget && !showCombatIdleCursor)
        {
            const Math::Vec2 mpos = mouseWorldPosForHover;
            auto pointOnAabb = [&](Math::Vec2 center, Math::Vec2 size) {
                return Collision::CheckPointInAABB(mpos, center, size)
                    || Collision::CheckAABB(mpos, kCursorHitboxCombat, center, size);
            };

            auto anyPulseGeom = [&](const std::vector<PulseSource>& sources) {
                for (const auto& src : sources)
                {
                    if (pointOnAabb(src.GetPosition(), src.GetHitboxSize()))
                        return true;
                }
                return false;
            };

            auto geomOnDrone = [&](const Drone& drone) {
                const Math::Vec2 hb = drone.GetSize() * droneHoverScale;
                return Collision::CheckPointInAABB(mpos, drone.GetPosition(), hb)
                    || Collision::CheckAABB(mpos, kCursorHitboxCombat, drone.GetPosition(), hb);
            };
            auto tryDroneList = [&](const std::vector<Drone>& ds) {
                for (const auto& d : ds)
                {
                    if (geomOnDrone(d))
                        return true;
                }
                return false;
            };
            auto geomOnRobot = [&](const Robot& robot) {
                const Math::Vec2 sz = robot.GetSize();
                return Collision::CheckPointInAABB(mpos, robot.GetPosition(), sz)
                    || Collision::CheckAABB(mpos, kCursorHitboxCombat, robot.GetPosition(), sz);
            };
            auto tryRobotList = [&](const std::vector<Robot>& rs) {
                for (const auto& r : rs)
                {
                    if (geomOnRobot(r))
                        return true;
                }
                return false;
            };

            if (anyPulseGeom(m_room->GetPulseSources()) || anyPulseGeom(m_hallway->GetPulseSources())
                || anyPulseGeom(m_rooftop->GetPulseSources()) || anyPulseGeom(m_underground->GetPulseSources())
                || anyPulseGeom(m_train->GetPulseSources()))
                showIdleOverWorldObject = true;

            if (!showIdleOverWorldObject && m_door && pointOnAabb(m_door->GetPosition(), m_door->GetSize()))
                showIdleOverWorldObject = true;
            if (!showIdleOverWorldObject && m_rooftopDoor && pointOnAabb(m_rooftopDoor->GetPosition(), m_rooftopDoor->GetSize()))
                showIdleOverWorldObject = true;
            if (!showIdleOverWorldObject && m_room && pointOnAabb(m_room->GetBlindPos(), m_room->GetBlindSize()))
                showIdleOverWorldObject = true;
            if (!showIdleOverWorldObject && m_rooftop && pointOnAabb(m_rooftop->GetHolePos(), m_rooftop->GetHoleSize()))
                showIdleOverWorldObject = true;
            if (!showIdleOverWorldObject && m_rooftop
                && pointOnAabb(m_rooftop->GetLiftButtonPos(), m_rooftop->GetLiftButtonSize()))
                showIdleOverWorldObject = true;

            if (!showIdleOverWorldObject && m_hallway
                && pointOnAabb(m_hallway->GetObstacleCenter(), m_hallway->GetObstacleSize()))
                showIdleOverWorldObject = true;
            if (!showIdleOverWorldObject && m_hallway)
            {
                for (const auto& spot : m_hallway->GetHidingSpots())
                {
                    if (pointOnAabb(spot.pos, spot.size))
                    {
                        showIdleOverWorldObject = true;
                        break;
                    }
                }
            }

            if (!showIdleOverWorldObject && m_undergroundAccessed && m_underground
                && m_underground->IsPointOverConfiguredGeometry(mpos, kCursorHitboxCombat))
                showIdleOverWorldObject = true;

            if (!showIdleOverWorldObject && m_train && m_train->IsPointOverWorldCollisionAABB(mpos, kCursorHitboxCombat))
                showIdleOverWorldObject = true;
            if (!showIdleOverWorldObject && m_trainAccessed && m_train)
            {
                const float tl = Train::MIN_X + m_train->GetTrainOffset();
                for (const auto& hs : m_train->GetHidingSpots())
                {
                    const Math::Vec2 wc = { tl + hs.localCenter.x, Train::MIN_Y + hs.localCenter.y };
                    if (pointOnAabb(wc, hs.size))
                    {
                        showIdleOverWorldObject = true;
                        break;
                    }
                }
            }

            if (!showIdleOverWorldObject && tryDroneList(droneManager->GetDrones()))
                showIdleOverWorldObject = true;
            if (!showIdleOverWorldObject)
            {
                for (const auto& d : m_hallway->GetDrones())
                {
                    if (geomOnDrone(d))
                    {
                        showIdleOverWorldObject = true;
                        break;
                    }
                }
            }
            if (!showIdleOverWorldObject)
            {
                for (const auto& d : m_rooftop->GetDrones())
                {
                    if (geomOnDrone(d))
                    {
                        showIdleOverWorldObject = true;
                        break;
                    }
                }
            }
            if (!showIdleOverWorldObject && m_undergroundAccessed)
            {
                for (const auto& d : m_underground->GetDrones())
                {
                    if (geomOnDrone(d))
                    {
                        showIdleOverWorldObject = true;
                        break;
                    }
                }
            }
            if (!showIdleOverWorldObject && m_trainAccessed && m_train)
            {
                for (const auto& d : m_train->GetDrones())
                {
                    if (geomOnDrone(d))
                    {
                        showIdleOverWorldObject = true;
                        break;
                    }
                }
                if (!showIdleOverWorldObject && m_train->GetCarTransportDroneManager())
                {
                    for (const auto& d : m_train->GetCarTransportDroneManager()->GetDrones())
                    {
                        if (geomOnDrone(d))
                        {
                            showIdleOverWorldObject = true;
                            break;
                        }
                    }
                }
                if (!showIdleOverWorldObject && m_train->GetSirenDroneManager())
                {
                    for (const auto& d : m_train->GetSirenDroneManager()->GetDrones())
                    {
                        if (geomOnDrone(d))
                        {
                            showIdleOverWorldObject = true;
                            break;
                        }
                    }
                }
            }

            if (!showIdleOverWorldObject && m_undergroundAccessed && tryRobotList(m_underground->GetRobots()))
                showIdleOverWorldObject = true;
            if (!showIdleOverWorldObject && m_trainAccessed && m_train && tryRobotList(m_train->GetRobots()))
                showIdleOverWorldObject = true;
        }
    }

    const bool showLeftCursor = overLeftClickTarget && !overRightClickTarget;
    const bool showRightCursor = overRightClickTarget && !overLeftClickTarget;
    const bool showMouseIdleCursor =
        storyDialogueBlocking
        || (!m_isDebugDraw && (showCombatIdleCursor || showIdleOverWorldObject));

    if (showLeftCursor || showRightCursor)
    {
        // Draw left/right cursor sprites
        GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        textureShader.use();
        textureShader.setMat4("projection", baseProjection);
        textureShader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
        textureShader.setBool("flipX", false);
        textureShader.setFloat("alpha", 1.0f);
        textureShader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
        textureShader.setFloat("tintStrength", 0.0f);

        const float iconSize = 32.0f;
        // Tilt both cursor images 15 degrees to the left.
        Math::Matrix iconModel =
            Math::Matrix::CreateTranslation(cursorPos) *
            Math::Matrix::CreateRotation(15.0f) *
            Math::Matrix::CreateScale({ iconSize, iconSize });

        if (showLeftCursor && m_mouseLeftCursor)
            m_mouseLeftCursor->Draw(textureShader, iconModel);
        else if (showRightCursor && m_mouseRightCursor)
            m_mouseRightCursor->Draw(textureShader, iconModel);
    }
    else if (m_mouseIdleCursor && showMouseIdleCursor)
    {
        // 스토리, 전투 호버 불가, 또는 상호작용 없는 월드 오브젝트 위
        GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        textureShader.use();
        textureShader.setMat4("projection", baseProjection);
        textureShader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
        textureShader.setBool("flipX", false);
        textureShader.setFloat("alpha", 1.0f);
        textureShader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
        textureShader.setFloat("tintStrength", 0.0f);

        const float iconSize = 32.0f;
        Math::Matrix iconModel =
            Math::Matrix::CreateTranslation(cursorPos) *
            Math::Matrix::CreateRotation(15.0f) *
            Math::Matrix::CreateScale({ iconSize, iconSize });
        m_mouseIdleCursor->Draw(textureShader, iconModel);
    }
    else if (!storyDialogueBlocking && m_mousePointerCursor && m_mousePointerCursor->GetWidth() > 0)
    {
        GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        textureShader.use();
        textureShader.setMat4("projection", baseProjection);
        textureShader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
        textureShader.setBool("flipX", false);
        textureShader.setFloat("alpha", 1.0f);
        textureShader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
        textureShader.setFloat("tintStrength", 0.0f);

        const float iconSize = 50.0f;
        Math::Matrix iconModel =
            Math::Matrix::CreateTranslation(cursorPos) *
            Math::Matrix::CreateScale({ iconSize, iconSize });
        m_mousePointerCursor->Draw(textureShader, iconModel);
    }
    else if (m_mouseIdleCursor)
    {
        GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        textureShader.use();
        textureShader.setMat4("projection", baseProjection);
        textureShader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
        textureShader.setBool("flipX", false);
        textureShader.setFloat("alpha", 1.0f);
        textureShader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
        textureShader.setFloat("tintStrength", 0.0f);

        const float iconSize = 32.0f;
        Math::Matrix iconModel =
            Math::Matrix::CreateTranslation(cursorPos) *
            Math::Matrix::CreateRotation(15.0f) *
            Math::Matrix::CreateScale({ iconSize, iconSize });
        m_mouseIdleCursor->Draw(textureShader, iconModel);
    }

    // 11) Debug overlay
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

        for (const auto& drone : m_train->GetDrones())
        {
            if (!drone.IsDead())
            {
                m_debugRenderer->DrawBox(*colorShader, drone.GetPosition(), drone.GetSize() * 0.8f, { 1.0f, 1.0f });
                m_debugRenderer->DrawCircle(*colorShader, drone.GetPosition(), Drone::DETECTION_RANGE, { 1.0f, 0.5f });
            }
        }

        if (m_train && m_train->GetCarTransportDroneManager())
        {
            for (const auto& drone : m_train->GetCarTransportDroneManager()->GetDrones())
            {
                if (!drone.IsDead())
                {
                    m_debugRenderer->DrawBox(*colorShader, drone.GetPosition(), drone.GetSize() * 0.8f, { 1.0f, 1.0f });
                    m_debugRenderer->DrawCircle(*colorShader, drone.GetPosition(), Drone::DETECTION_RANGE, { 1.0f, 0.5f });
                }
            }
        }

        if (m_train && m_train->GetSirenDroneManager())
        {
            for (const auto& drone : m_train->GetSirenDroneManager()->GetDrones())
            {
                if (!drone.IsDead())
                {
                    m_debugRenderer->DrawBox(*colorShader, drone.GetPosition(), drone.GetSize() * 0.8f, { 1.0f, 1.0f });
                    m_debugRenderer->DrawCircle(*colorShader, drone.GetPosition(), Drone::DETECTION_RANGE, { 1.0f, 0.5f });
                }
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
        m_train->DrawDebug(*colorShader, *m_debugRenderer);
        m_door->DrawDebug(*colorShader);
        m_rooftopDoor->DrawDebug(*colorShader);
    }

    // Black fade overlay for map transitions
    if (m_fadeAlpha > 0.0f && m_fadeVAO != 0)
    {
        GL::Enable(GL_BLEND);
        GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        Math::Matrix fadeProj = Math::Matrix::CreateOrtho(0.0f, GAME_WIDTH, 0.0f, GAME_HEIGHT, -1.0f, 1.0f);
        Math::Matrix fadeModel = Math::Matrix::CreateTranslation({ GAME_WIDTH * 0.5f, GAME_HEIGHT * 0.5f })
                               * Math::Matrix::CreateScale({ GAME_WIDTH, GAME_HEIGHT });
        colorShader->use();
        colorShader->setMat4("projection", fadeProj);
        colorShader->setMat4("model", fadeModel);
        colorShader->setVec3("objectColor", 0.0f, 0.0f, 0.0f);
        colorShader->setFloat("uAlpha", m_fadeAlpha);
        GL::BindVertexArray(m_fadeVAO);
        GL::DrawArrays(GL_TRIANGLES, 0, 6);
        GL::BindVertexArray(0);
    }

    GL::Disable(GL_BLEND);
}

void GameplayState::Shutdown()
{
    auto& pp = gsm.GetEngine().GetPostProcess();
    pp.Settings() = PostProcessSettings{};
    pp.SetPassthrough(false);

    if (auto* imguiManager = gsm.GetEngine().GetImguiManager())
    {
        imguiManager->ClearGameplayBindings();
    }

    m_room->Shutdown();
    m_hallway->Shutdown();
    m_rooftop->Shutdown();
    m_underground->Shutdown();
    m_train->Shutdown();
    player.Shutdown();
    droneManager->Shutdown();
    m_pulseGauge.Shutdown();
    m_debugRenderer->Shutdown();
    m_door->Shutdown();
    m_rooftopDoor->Shutdown();
    pulseManager->Shutdown();

    if (m_mouseIdleCursor) m_mouseIdleCursor->Shutdown();
    if (m_mousePointerCursor) m_mousePointerCursor->Shutdown();
    if (m_mouseLeftCursor) m_mouseLeftCursor->Shutdown();
    if (m_mouseRightCursor) m_mouseRightCursor->Shutdown();
    if (m_hudFrame) m_hudFrame->Shutdown();
    if (m_hallwayHidingPromptS) m_hallwayHidingPromptS->Shutdown();

    if (m_storyDialogue) m_storyDialogue->Shutdown();

    if (m_fadeVAO != 0)
    {
        GL::DeleteVertexArrays(1, &m_fadeVAO);
        GL::DeleteBuffers(1, &m_fadeVBO);
        m_fadeVAO = 0;
        m_fadeVBO = 0;
    }

    m_bgm.Stop();

    Logger::Instance().Log(Logger::Severity::Info, "GameplayState Shutdown");
}

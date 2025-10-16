#include "GameplayState.hpp"
#include "../Engine/GameStateManager.hpp"
#include "../Engine/Engine.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Logger.hpp"
#include "../Engine/Matrix.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

constexpr float GROUND_LEVEL = 350.0f;
constexpr float VISUAL_Y_OFFSET = 55.0f;
constexpr float ATTACK_RANGE = 200.0f;
constexpr float ATTACK_RANGE_SQ = ATTACK_RANGE * ATTACK_RANGE;

GameplayState::GameplayState(GameStateManager& gsm_ref) : gsm(gsm_ref) {}

void GameplayState::Initialize()
{
    Logger::Instance().Log(Logger::Severity::Info, "GameplayState Initialize");
    textureShader = std::make_unique<Shader>("OpenGL/shaders/simple.vert", "OpenGL/shaders/simple.frag");
    textureShader->use();
    textureShader->setInt("imageTexture", 0);
    colorShader = std::make_unique<Shader>("OpenGL/shaders/solid_color.vert", "OpenGL/shaders/solid_color.frag");

    float line_vertices[] = { -0.5f, 0.5f, 0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f, -0.5f, -0.5f, -0.5f };
    glGenVertexArrays(1, &groundVAO);
    glGenBuffers(1, &groundVBO);
    glBindVertexArray(groundVAO);
    glBindBuffer(GL_ARRAY_BUFFER, groundVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(line_vertices), line_vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    player.Init({ 200.0f, GROUND_LEVEL + 400.0f }, "Asset/player.png");

    pulseManager = std::make_unique<PulseManager>();

    pulseSources.emplace_back();
    pulseSources.back().Initialize({ 600.f, GROUND_LEVEL + 50.f + VISUAL_Y_OFFSET }, { 50.f, 50.f }, 100.f);
    pulseSources.emplace_back();
    pulseSources.back().Initialize({ 800.f, GROUND_LEVEL + 150.f + VISUAL_Y_OFFSET }, { 30.f, 80.f }, 150.f);
    pulseSources.emplace_back();
    pulseSources.back().Initialize({ 1200.f, GROUND_LEVEL + 80.f + VISUAL_Y_OFFSET }, { 60.f, 60.f }, 120.f);
    pulseSources.emplace_back();
    pulseSources.back().Initialize({ 1500.f, GROUND_LEVEL + 200.f + VISUAL_Y_OFFSET }, { 40.f, 100.f }, 200.f);

    droneManager = std::make_unique<DroneManager>();
    droneManager->SpawnDrone({ 800.0f, GROUND_LEVEL + 130.0f }, "Asset/Drone.png");

    Engine& engine = gsm.GetEngine();
    m_pulseGauge.Initialize({ 80.f, engine.GetHeight() * 0.75f }, { 40.f, 300.f });

    m_debugRenderer = std::make_unique<DebugRenderer>();
    m_debugRenderer->Initialize();
}

void GameplayState::Update(double dt)
{
    Engine& engine = gsm.GetEngine();
    GLFWwindow* window = engine.GetWindow();

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) { glfwSetWindowShouldClose(window, true); }

    m_isDebugDraw = (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS);

   
    Math::Vec2 playerPos = player.GetPosition();
    Math::Vec2 playerSize = player.GetSize();
    Math::Vec2 playerCenter = { playerPos.x + 60.0f, playerPos.y + playerSize.y / 1.1f };

    if (true)
    {
        Logger::Instance().Log(Logger::Severity::Debug, "Player Pulse: %.1f / %.1f",
            player.GetPulseCore().getPulse().Value(),
            player.GetPulseCore().getPulse().Max());

        bool isPressingE = (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS);
        pulseManager->Update(player, pulseSources, isPressingE, dt);

        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) player.MoveLeft();
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) player.MoveRight();
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) player.Jump();
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) player.Crouch();
        else player.StopCrouch();
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) player.Dash();

        static bool f_key_was_pressed = false;
        bool f_key_is_pressed = (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS);
        if (f_key_is_pressed && !f_key_was_pressed)
        {
            if (player.GetPulseCore().getPulse().Value() >= 20.0f)
            {
                auto& drones = droneManager->GetDrones();
                for (auto& drone : drones)
                {
                    // 위에서 계산한 playerCenter를 사용하여 공격 사거리를 판정합니다.
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
        f_key_was_pressed = f_key_is_pressed;
    }

    droneManager->Update(dt);
    player.Update(dt);

    const auto& drones = droneManager->GetDrones();
    for (const auto& drone : drones)
    {
        Math::Vec2 playerHitboxSize = { playerSize.x * 0.4f, playerSize.y * 0.8f };
        Math::Vec2 droneHitboxSize = drone.GetSize() * 0.8f;

        // [수정] playerCenter를 기준으로 플레이어의 히트박스(AABB)를 계산합니다.
        float playerMinX = playerCenter.x - playerHitboxSize.x / 2.0f;
        float playerMaxX = playerCenter.x + playerHitboxSize.x / 2.0f;
        float playerMinY = playerCenter.y - playerHitboxSize.y / 2.0f;
        float playerMaxY = playerCenter.y + playerHitboxSize.y / 2.0f;

        Math::Vec2 dronePos = drone.GetPosition();
        float droneMinX = dronePos.x - droneHitboxSize.x / 2.0f;
        float droneMaxX = dronePos.x + droneHitboxSize.x / 2.0f;
        float droneMinY = dronePos.y - droneHitboxSize.y / 2.0f;
        float droneMaxY = dronePos.y + droneHitboxSize.y / 2.0f;

        if (playerMaxX > droneMinX && playerMinX < droneMaxX &&
            playerMaxY > droneMinY && playerMinY < droneMaxY)
        {
            player.TakeDamage(20.0f);
            break;
        }
    }

    const auto& pulse = player.GetPulseCore().getPulse();
    m_pulseGauge.Update(pulse.Value(), pulse.Max());
}

void GameplayState::Draw()
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    Engine& engine = gsm.GetEngine();
    Math::Matrix projection = Math::Matrix::CreateOrtho(0.0f, static_cast<float>(engine.GetWidth()), 0.0f, static_cast<float>(engine.GetHeight()), -1.0f, 1.0f);

    colorShader->use();
    colorShader->setMat4("projection", projection);
    for (const auto& source : pulseSources) {
        source.Draw(*colorShader);
    }
    Math::Vec2 groundPosition = { engine.GetWidth() / 2.0f, GROUND_LEVEL + VISUAL_Y_OFFSET };
    Math::Vec2 groundSize = { static_cast<float>(engine.GetWidth()), 2.0f };
    Math::Matrix groundModel = Math::Matrix::CreateTranslation(groundPosition) * Math::Matrix::CreateScale(groundSize);
    colorShader->setMat4("model", groundModel);
    colorShader->setVec3("objectColor", 0.8f, 0.8f, 0.8f);
    glBindVertexArray(groundVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    textureShader->use();
    textureShader->setMat4("projection", projection);
    droneManager->Draw(*textureShader);
    player.Draw(*textureShader);

    colorShader->use();
    m_pulseGauge.Draw(*colorShader);

    if (m_isDebugDraw)
    {
        colorShader->use();

        // 플레이어의 중심을 기준으로 사거리와 히트박스를 그리자
        Math::Vec2 playerPos = player.GetPosition();
        Math::Vec2 playerSize = player.GetSize();
        Math::Vec2 playerCenter = { playerPos.x + 60.0f, playerPos.y + playerSize.y / 1.1f };

        // 플레이어 공격 사거리 그리기
        m_debugRenderer->DrawCircle(*colorShader, playerCenter, ATTACK_RANGE * 0.7f, { 0.2f, 0.8f });

        // 플레이어 히트박스 그리기
        m_debugRenderer->DrawBox(*colorShader, playerCenter, { playerSize.x * 0.4f, playerSize.y * 0.8f }, { 0.2f, 0.2f });

        // 드론 히트박스 그리기
        const auto& drones = droneManager->GetDrones();
        for (const auto& drone : drones)
        {
            // 드론의 위치 기준점은 이미 중앙이므로 그대로 사용
            m_debugRenderer->DrawBox(*colorShader, drone.GetPosition(), drone.GetSize() * 0.8f, { 0.8f, 0.2f });
        }
    }
}

void GameplayState::Shutdown()
{
    player.Shutdown();
    glDeleteVertexArrays(1, &groundVAO);
    glDeleteBuffers(1, &groundVBO);
    for (auto& source : pulseSources) {
        source.Shutdown();
    }
    droneManager->Shutdown();
    m_pulseGauge.Shutdown();
    m_debugRenderer->Shutdown();
    Logger::Instance().Log(Logger::Severity::Info, "GameplayState Shutdown");
}
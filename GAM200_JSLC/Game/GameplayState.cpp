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
constexpr float ATTACK_RANGE_SQ = 250.0f * 250.0f;

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

    player.Init({ 200.0f, GROUND_LEVEL + 400.0f}, "Asset/player.png");

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
    droneManager->SpawnDrone({ 800.0f, GROUND_LEVEL + 70.0f }, "Asset/Drone.png");

    Engine& engine = gsm.GetEngine();
    m_pulseGauge.Initialize({ 80.f, engine.GetHeight() * 0.75f }, { 40.f, 300.f });
}

void GameplayState::Update(double dt)
{
    Engine& engine = gsm.GetEngine();
    GLFWwindow* window = engine.GetWindow();

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) { glfwSetWindowShouldClose(window, true); }

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
                float distSq = (player.GetPosition() - drone.GetPosition()).LengthSq();
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

    droneManager->Update(dt);
    player.Update(dt);

    const auto& drones = droneManager->GetDrones();
    for (const auto& drone : drones)
    {
        Math::Vec2 playerPos = player.GetPosition();
        Math::Vec2 playerSize = player.GetSize();
        float playerMinX = playerPos.x - playerSize.x / 2.0f;
        float playerMaxX = playerPos.x + playerSize.x / 2.0f;
        float playerMinY = playerPos.y;
        float playerMaxY = playerPos.y + playerSize.y;

        Math::Vec2 dronePos = drone.GetPosition();
        Math::Vec2 droneSize = drone.GetSize();
        float droneMinX = dronePos.x - droneSize.x / 2.0f;
        float droneMaxX = dronePos.x + droneSize.x / 2.0f;
        float droneMinY = dronePos.y;
        float droneMaxY = dronePos.y + droneSize.y;

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
    Logger::Instance().Log(Logger::Severity::Info, "GameplayState Shutdown");
}
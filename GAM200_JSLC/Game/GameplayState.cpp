#include "GameplayState.hpp"
#include "../Engine/GameStateManager.hpp"
#include "../Engine/Engine.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Logger.hpp"
#include "../Engine/Matrix.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include <GLFW/glfw3.h>

// [수정] 바닥 높이를 170.0f로 변경
constexpr float GROUND_LEVEL = 170.0f;
constexpr float VISUAL_Y_OFFSET = 0.0f; // Room.png에 바닥이 있으므로 0으로 설정
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

    // Room.png에 바닥이 있으므로, 하얀색 바닥선 VAO는 생성/사용하지 않음 (주석 처리)
    /*
    float line_vertices[] = { -0.5f, 0.5f, 0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f, -0.5f, -0.5f, -0.5f };
    GL::GenVertexArrays(1, &groundVAO);
    GL::GenBuffers(1, &groundVBO);
    GL::BindVertexArray(groundVAO);
    GL::BindBuffer(GL_ARRAY_BUFFER, groundVBO);
    GL::BufferData(GL_ARRAY_BUFFER, sizeof(line_vertices), line_vertices, GL_STATIC_DRAW);
    GL::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    GL::EnableVertexAttribArray(0);
    GL::BindBuffer(GL_ARRAY_BUFFER, 0);
    GL::BindVertexArray(0);
    */

    // [수정] 플레이어 시작 위치를 새 GROUND_LEVEL 기준으로 변경
    player.Init({ 300.0f, GROUND_LEVEL + 100.0f }, "Asset/player.png");

    pulseManager = std::make_unique<PulseManager>();

    // [수정] 펄스 공급원을 요청하신 위치에 하나만 생성합니다.
    pulseSources.emplace_back();
    pulseSources.back().Initialize({ 420.f, 390.f }, { 50.f, 50.f }, 100.f);

    droneManager = std::make_unique<DroneManager>();
    // [수정] 드론들을 1620px 방(X: 180 ~ 1800) 안으로 재배치
    droneManager->SpawnDrone({ 800.0f, GROUND_LEVEL + 330.0f }, "Asset/Drone.png");
    droneManager->SpawnDrone({ 1200.0f, GROUND_LEVEL + 430.0f }, "Asset/Drone.png");
    droneManager->SpawnDrone({ 1600.0f, GROUND_LEVEL + 350.0f }, "Asset/Drone.png");

    Engine& engine = gsm.GetEngine();
    m_pulseGauge.Initialize({ 80.f, engine.GetHeight() * 0.75f }, { 40.f, 300.f });

    m_debugRenderer = std::make_unique<DebugRenderer>();
    m_debugRenderer->Initialize();

    // Background 초기화
    m_background = std::make_unique<Background>();
    m_background->Initialize("Asset/Room.png");
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
        m_logTimer += dt;
        if (m_logTimer >= 0.5)
        {
            Logger::Instance().Log(Logger::Severity::Debug, "Player Pulse: %.1f / %.1f",
                player.GetPulseCore().getPulse().Value(),
                player.GetPulseCore().getPulse().Max());
            m_logTimer -= 0.5;
        }

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

    // [수정] 카메라 업데이트 호출 삭제
    // m_camera->Update(player.GetPosition(), dt);

    // [수정] 'Room.png'의 내부 경계(1620x660)로 플레이어 위치를 제한
    Math::Vec2 currentPlayerPos = player.GetPosition();
    float screenWidth = static_cast<float>(engine.GetWidth()); // 1980

    const float roomWidth = 1620.0f;
    const float roomHeight = 660.0f;
    const float minX = (screenWidth - roomWidth) / 2.0f; // (1980 - 1620) / 2 = 180.0f
    const float maxX = minX + roomWidth;                  // 180.0f + 1620.0f = 1800.0f
    const float minY = GROUND_LEVEL;                      // 170.0f
    const float maxY = minY + roomHeight;                 // 170.0f + 660.0f = 830.0f

    // X축 경계 체크
    if (currentPlayerPos.x < minX)
    {
        player.SetPosition({ minX, currentPlayerPos.y });
    }
    else if (currentPlayerPos.x + player.GetSize().x > maxX)
    {
        player.SetPosition({ maxX - player.GetSize().x, currentPlayerPos.y });
    }

    // Y축 경계 체크 (천장)
    if (currentPlayerPos.y + player.GetSize().y > maxY)
    {
        player.SetPosition({ currentPlayerPos.x, maxY - player.GetSize().y });
    }
}

void GameplayState::Draw()
{
    GL::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    Engine& engine = gsm.GetEngine();

    // [수정] 카메라 View 행렬 제거. 'projection'만 사용합니다.
    Math::Matrix projection = Math::Matrix::CreateOrtho(0.0f, static_cast<float>(engine.GetWidth()), 0.0f, static_cast<float>(engine.GetHeight()), -1.0f, 1.0f);

    // --- 1. 배경 그리기 (projection 사용) ---
    textureShader->use();
    textureShader->setMat4("projection", projection);
    Math::Vec2 screenSize = { (float)engine.GetWidth(), (float)engine.GetHeight() };
    Math::Vec2 screenCenter = screenSize * 0.5f;
    Math::Matrix bg_model = Math::Matrix::CreateTranslation(screenCenter) * Math::Matrix::CreateScale(screenSize);
    m_background->Draw(*textureShader, bg_model);

    // --- 2. 월드 객체 그리기 (projection 사용) ---
    colorShader->use();
    colorShader->setMat4("projection", projection);
    for (const auto& source : pulseSources) {
        source.Draw(*colorShader);
    }

    // [수정] 하얀색 바닥선 그리기 주석 처리
    /*
    Math::Vec2 groundPosition = { 2500.0f, GROUND_LEVEL + VISUAL_Y_OFFSET };
    Math::Vec2 groundSize = { 5000.0f, 2.0f };
    Math::Matrix groundModel = Math::Matrix::CreateTranslation(groundPosition) * Math::Matrix::CreateScale(groundSize);
    colorShader->setMat4("model", groundModel);
    colorShader->setVec3("objectColor", 0.8f, 0.8f, 0.8f);
    GL::BindVertexArray(groundVAO);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);
    GL::BindVertexArray(0);
    */

    textureShader->use();
    textureShader->setMat4("projection", projection);
    droneManager->Draw(*textureShader);
    player.Draw(*textureShader);

    // --- 3. UI 그리기 (projection 사용) ---
    colorShader->use();
    colorShader->setMat4("projection", projection);
    m_pulseGauge.Draw(*colorShader);

    // --- 4. 디버그 그리기 (projection 사용) ---
    if (m_isDebugDraw)
    {
        colorShader->use();
        colorShader->setMat4("projection", projection);

        // [수정] 방 경계를 그리는 디버그 박스 추가
        const float roomWidth = 1620.0f;
        const float roomHeight = 660.0f;
        const float minX = (engine.GetWidth() - roomWidth) / 2.0f;
        Math::Vec2 roomCenter = { minX + roomWidth / 2.0f, GROUND_LEVEL + roomHeight / 2.0f };
        m_debugRenderer->DrawBox(*colorShader, roomCenter, { roomWidth, roomHeight }, { 1.0f, 0.0f });

        // (플레이어, 드론 히트박스 그리는 코드는 동일)
        Math::Vec2 playerPos = player.GetPosition();
        Math::Vec2 playerSize = player.GetSize();
        Math::Vec2 playerCenter = { playerPos.x + 60.0f, playerPos.y + playerSize.y / 1.1f };
        m_debugRenderer->DrawCircle(*colorShader, playerCenter, ATTACK_RANGE, { 1.0f, 0.0f });
        m_debugRenderer->DrawBox(*colorShader, playerCenter, { playerSize.x * 0.4f, playerSize.y * 0.8f }, { 0.0f, 1.0f });
        const auto& drones = droneManager->GetDrones();
        for (const auto& drone : drones)
        {
            m_debugRenderer->DrawBox(*colorShader, drone.GetPosition(), drone.GetSize() * 0.8f, { 1.0f, 1.0f });
        }
    }
}

void GameplayState::Shutdown()
{
    m_background->Shutdown();
    player.Shutdown();
    // GL::DeleteVertexArrays(1, &groundVAO); // 주석 처리
    // GL::DeleteBuffers(1, &groundVBO); // 주석 처리
    for (auto& source : pulseSources) {
        source.Shutdown();
    }
    droneManager->Shutdown();
    m_pulseGauge.Shutdown();
    m_debugRenderer->Shutdown();
    Logger::Instance().Log(Logger::Severity::Info, "GameplayState Shutdown");
}   
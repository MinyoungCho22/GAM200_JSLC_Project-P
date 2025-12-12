#include "MainMenu.hpp"
#include "../Engine/GameStateManager.hpp"
#include "../Engine/Input.hpp"
#include "../Engine/Engine.hpp"
#include "../OpenGL/Shader.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include "../Engine/Matrix.hpp" 
#include "GameplayState.hpp" 
#include <cmath>
#include <string>
#include <vector>
#include <random>

constexpr float GAME_WIDTH = 1920.0f;
constexpr float GAME_HEIGHT = 1080.0f;

MainMenu::MainMenu(GameStateManager& gsm_ref) : gsm(gsm_ref) {}

void MainMenu::Initialize()
{
    m_fontShader = std::make_unique<Shader>("OpenGL/shaders/simple.vert", "OpenGL/shaders/simple.frag");
    m_font = std::make_unique<Font>();
    m_font->Initialize("Asset/fonts/Font_Outlined.png");

    m_promptText = m_font->PrintToTexture(*m_fontShader, "Press ENTER to help the City!");
    m_shapeShader = std::make_unique<Shader>("OpenGL/shaders/solid_color.vert", "OpenGL/shaders/solid_color.frag");

    float vertices[] = {
        0.0f, 0.0f, // Bottom-Left
        1.0f, 0.0f, // Bottom-Right
        0.0f, 1.0f, // Top-Left

        0.0f, 1.0f, // Top-Left
        1.0f, 0.0f, // Bottom-Right
        1.0f, 1.0f  // Top-Right
    };

    GL::GenVertexArrays(1, &m_shapeVAO);
    GL::GenBuffers(1, &m_shapeVBO);
    GL::BindVertexArray(m_shapeVAO);
    GL::BindBuffer(GL_ARRAY_BUFFER, m_shapeVBO);
    GL::BufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    GL::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    GL::EnableVertexAttribArray(0);
    GL::BindVertexArray(0);
}

void MainMenu::Update(double dt)
{
    m_glitchTimer += dt;

    if (gsm.GetEngine().GetInput().IsKeyTriggered(Input::Key::Enter))
    {
        gsm.ChangeState(std::make_unique<GameplayState>(gsm));
    }
}

void MainMenu::Draw()
{
    // 배경 짙은 네이비
    GL::ClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    GL::Clear(GL_COLOR_BUFFER_BIT);

    Math::Matrix projection = Math::Matrix::CreateOrtho(0.0f, GAME_WIDTH, 0.0f, GAME_HEIGHT, -1.0f, 1.0f);

    m_shapeShader->use();
    m_shapeShader->setMat4("projection", projection);

    GL::BindVertexArray(m_shapeVAO);

    auto DrawRect = [&](float x, float y, float w, float h, float r, float g, float b) {
        Math::Matrix model = Math::Matrix::CreateTranslation({ x, y }) * Math::Matrix::CreateScale({ w, h });
        m_shapeShader->setMat4("model", model);
        m_shapeShader->setVec3("objectColor", r, g, b);
        GL::DrawArrays(GL_TRIANGLES, 0, 6);
        };

    auto DrawLetter = [&](char letter, float x, float y, float size, float r, float g, float b) {
        float t = size * 0.15f;
        float h = size; 
        float w = size * 0.7f;

        switch (letter) {
        case 'N':
            // 좌측 기둥
            DrawRect(x, y, t, h, r, g, b);
            // 우측 기둥
            DrawRect(x + w - t, y, t, h, r, g, b);

            // N의 대각선
            // 상단 작은 블록
            DrawRect(x + t, y + h * 0.6f, t, h * 0.2f, r, g, b);
            // 중간 블록
            DrawRect(x + t * 1.5f, y + h * 0.4f, t, h * 0.2f, r, g, b);
            // 하단 작은 블록 (우측 기둥과 연결)
            DrawRect(x + t * 2.5f, y + h * 0.2f, t * 0.5f, h * 0.2f, r, g, b);
            break;
        case 'o':
   
            DrawRect(x, y, w, t, r, g, b);          // 하단 (바닥에 붙임)
            DrawRect(x, y + h - t, w, t, r, g, b);  // 상단
            DrawRect(x, y, t, h, r, g, b);          // 좌측 기둥 (전체 높이)
            DrawRect(x + w - t, y, t, h, r, g, b);  // 우측 기둥 (전체 높이)
            break;
        case '.':
            DrawRect(x + t, y, t * 1.5f, t * 1.5f, r, g, b); // 바닥 점
            break;
        case '9':
            DrawRect(x, y + h - t, w, t, r, g, b);          // 상단
            DrawRect(x, y + h / 2, t, h / 2, r, g, b);      // 좌측 상단
            DrawRect(x + w - t, y, t, h, r, g, b);          // 우측 전체
            DrawRect(x, y + h / 2, w, t, r, g, b);          // 중간
            DrawRect(x, y, w, t, r, g, b);                  // 하단
            break;
        case ' ': // 공백
            break;
        }
        };

    std::string title = "No. 99";

    // 중앙 정렬 계산
    float fontSize = 150.0f;
    float spacing = fontSize * 0.8f;
    float startX = (GAME_WIDTH - (title.length() * spacing)) / 2.0f;
    float startY = GAME_HEIGHT / 2.0f + 100.0f;

    // 글리치 효과 계산
    bool isGlitch = (rand() % 100) < 5; // 5% 확률
    float glitchX = 0.0f;
    float glitchY = 0.0f;

    if (isGlitch)
    {
        glitchX = (float)(rand() % 20 - 10);
        glitchY = (float)(rand() % 20 - 10);
    }

    // 그림자 레이어 (네온 핑크)
    for (size_t i = 0; i < title.length(); ++i)
    {
        float x = startX + i * spacing + glitchX;
        float y = startY + glitchY;
        DrawLetter(title[i], x + 8.0f, y - 8.0f, fontSize, 1.0f, 0.0f, 1.0f);
    }

    // 메인 레이어 (네온 민트)
    for (size_t i = 0; i < title.length(); ++i)
    {
        float x = startX + i * spacing;
        float y = startY;
        DrawLetter(title[i], x, y, fontSize, 0.0f, 1.0f, 1.0f);
    }

    GL::BindVertexArray(0);

    // 안내 문구
    GL::Enable(GL_BLEND);
    GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_fontShader->use();
    m_fontShader->setMat4("projection", projection);

    float pulse = (static_cast<float>(std::sin(m_glitchTimer * 3.0f)) + 1.0f) * 0.5f;
    float alpha = pulse * 0.8f + 0.2f;
    m_fontShader->setFloat("alpha", alpha);

    float promptHeight = 40.0f;
    float promptWidth = m_promptText.width * (promptHeight / m_font->m_fontHeight);
    Math::Vec2 promptPos = {
        (GAME_WIDTH - promptWidth) / 2.0f,
        (GAME_HEIGHT / 2.0f) - 300.0f
    };
    m_font->DrawBakedText(*m_fontShader, m_promptText, promptPos, promptHeight);

    GL::Disable(GL_BLEND);
}

void MainMenu::Shutdown()
{
    if (m_font) m_font->Shutdown();
    if (m_fontShader) m_fontShader.reset();

    if (m_shapeShader) m_shapeShader.reset();
    GL::DeleteVertexArrays(1, &m_shapeVAO);
    GL::DeleteBuffers(1, &m_shapeVBO);
}
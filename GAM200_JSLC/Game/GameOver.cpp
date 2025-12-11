#include "GameOver.hpp"
#include "MainMenu.hpp"
#include "../Engine/GameStateManager.hpp"
#include "../Engine/Engine.hpp"
#include "../OpenGL/Shader.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include "../Engine/Matrix.hpp"
#include <vector>
#include <cmath>
#include <string>
#include <random>

constexpr float GAME_WIDTH = 1920.0f;
constexpr float GAME_HEIGHT = 1080.0f;

GameOver::GameOver(GameStateManager& gsm_ref, bool& isGameOverFlag)
    : gsm(gsm_ref), m_isGameOverFlag(isGameOverFlag) {
    m_glitchTimer = 0.0;
}

void GameOver::Initialize()
{
    m_fontShader = std::make_unique<Shader>("OpenGL/shaders/simple.vert", "OpenGL/shaders/simple.frag");
    m_colorShader = std::make_unique<Shader>("OpenGL/shaders/solid_color.vert", "OpenGL/shaders/solid_color.frag");

    m_font = std::make_unique<Font>();
    m_font->Initialize("Asset/fonts/Font_Outlined.png");

    m_promptText = m_font->PrintToTexture(*m_fontShader, "Press ENTER to return to Main Menu");

    // 1x1 단위 사각형 (0,0 ~ 1,1)
    std::vector<float> vertices = {
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f
    };

    GL::GenVertexArrays(1, &m_quadVAO);
    GL::GenBuffers(1, &m_quadVBO);
    GL::BindVertexArray(m_quadVAO);
    GL::BindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    GL::BufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    GL::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    GL::EnableVertexAttribArray(0);
    GL::BindVertexArray(0);
}

void GameOver::Update(double dt)
{
    if (m_isExiting) return;

    m_glitchTimer += dt;

    if (gsm.GetEngine().GetInput().IsKeyTriggered(Input::Key::Enter))
    {
        m_isExiting = true;
        m_isGameOverFlag = true;
        gsm.PopState();
        return;
    }
}

void GameOver::Draw()
{
    Math::Matrix projection = Math::Matrix::CreateOrtho(0.0f, GAME_WIDTH, 0.0f, GAME_HEIGHT, -1.0f, 1.0f);

    GL::Enable(GL_BLEND);
    GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_colorShader->use();
    m_colorShader->setMat4("projection", projection);

    GL::BindVertexArray(m_quadVAO);

    auto DrawRect = [&](float x, float y, float w, float h, float r, float g, float b) {
        Math::Matrix model = Math::Matrix::CreateTranslation({ x, y }) * Math::Matrix::CreateScale({ w, h });
        m_colorShader->setMat4("model", model);
        m_colorShader->setVec3("objectColor", r, g, b);
        GL::DrawArrays(GL_TRIANGLES, 0, 6);
        };

    auto DrawLetter = [&](char letter, float x, float y, float size, float r, float g, float b) {
        float t = size * 0.15f; // 두께
        float h = size;         // 높이
        float w = size * 0.7f;  // 너비

        switch (letter) {
        case 'T':
            DrawRect(x, y + h - t, w, t, r, g, b);      // 상단 가로바
            DrawRect(x + w / 2 - t / 2, y, t, h, r, g, b); // 중간 기둥
            break;
        case 'E':
            DrawRect(x, y, t, h, r, g, b);          // 좌측 기둥
            DrawRect(x, y + h - t, w, t, r, g, b);  // 상단 바
            DrawRect(x, y + h * 0.5f - t * 0.5f, w * 0.8f, t, r, g, b); // 중간 바 (약간 짧게)
            DrawRect(x, y, w, t, r, g, b);          // 하단 바
            break;
        case 'R':
            DrawRect(x, y, t, h, r, g, b);          // 좌측 기둥
            DrawRect(x, y + h - t, w, t, r, g, b);  // 상단 바
            DrawRect(x + w - t, y + h * 0.5f, t, h * 0.5f, r, g, b); // 우측 상단 둥근 부분
            DrawRect(x, y + h * 0.5f, w, t, r, g, b); // 중간 바
            DrawRect(x + w - t, y, t, h * 0.5f, r, g, b); // 우측 하단 다리
            break;
        case 'M':
            DrawRect(x, y, t, h, r, g, b);          // 좌측 기둥
            DrawRect(x + w - t, y, t, h, r, g, b);  // 우측 기둥
            // M자 가운데 V 모양
            DrawRect(x + t, y + h - t, t, t, r, g, b); // 좌측 상단 안쪽
            DrawRect(x + w - 2 * t, y + h - t, t, t, r, g, b); // 우측 상단 안쪽
            DrawRect(x + 2 * t, y + h - 2 * t, w - 4 * t, t, r, g, b); // 가운데 연결
            break;
        case 'I':
            DrawRect(x + w * 0.5f - t * 0.5f, y, t, h, r, g, b); // 가운데 기둥
            DrawRect(x, y + h - t, w, t, r, g, b);  // 상단 받침
            DrawRect(x, y, w, t, r, g, b);          // 하단 받침
            break;
        case 'N':
            DrawRect(x, y, t, h, r, g, b);          // 좌측 기둥
            DrawRect(x + w - t, y, t, h, r, g, b);  // 우측 기둥
            DrawRect(x + t, y + h * 0.6f, t, h * 0.2f, r, g, b);
            DrawRect(x + t * 2.0f, y + h * 0.3f, t, h * 0.2f, r, g, b);
            break;
        case 'A':
            DrawRect(x, y, t, h, r, g, b);          // 좌측
            DrawRect(x + w - t, y, t, h, r, g, b);  // 우측
            DrawRect(x, y + h - t, w, t, r, g, b);  // 상단
            DrawRect(x, y + h * 0.5f, w, t, r, g, b); // 중간
            break;
        case 'D':
            DrawRect(x, y, t, h, r, g, b);          // 좌측 기둥
            DrawRect(x, y + h - t, w - t, t, r, g, b); // 상단 바
            DrawRect(x, y, w - t, t, r, g, b);      // 하단 바
            DrawRect(x + w - t, y + t, t, h - 2 * t, r, g, b); // 우측 닫기
            break;
        }
        };

    std::string title = "TERMINATED";
    float fontSize = 160.0f;
    float spacing = fontSize * 0.8f;
    float startX = (GAME_WIDTH - (title.length() * spacing)) / 2.0f;
    float startY = GAME_HEIGHT / 2.0f - 30.0f;


    bool isGlitch = (rand() % 100) < 5;
    float glitchX = 0.0f;
    float glitchY = 0.0f;
    if (isGlitch) {
        glitchX = (float)(rand() % 30 - 15);
        glitchY = (float)(rand() % 30 - 15);
    }


    for (size_t i = 0; i < title.length(); ++i) {
        float x = startX + i * spacing + glitchX + 8.0f;
        float y = startY + glitchY - 8.0f;
        DrawLetter(title[i], x, y, fontSize, 1.0f, 0.0f, 1.0f);
    }


    for (size_t i = 0; i < title.length(); ++i) {
        float x = startX + i * spacing;
        float y = startY;
        DrawLetter(title[i], x, y, fontSize, 0.0f, 1.0f, 1.0f);
    }

    GL::BindVertexArray(0);

    m_fontShader->use();
    m_fontShader->setMat4("projection", projection);

    float pulse = (static_cast<float>(std::sin(m_glitchTimer * 4.0f)) + 1.0f) * 0.5f;
    float alpha = pulse * 0.8f + 0.2f;
    m_fontShader->setFloat("alpha", alpha);

    float promptHeight = 40.0f;
    float promptWidth = m_promptText.width * (promptHeight / m_font->m_fontHeight);
    Math::Vec2 promptPos = { (GAME_WIDTH - promptWidth) / 2.0f, (GAME_HEIGHT / 2.0f) - 200.0f };
    m_font->DrawBakedText(*m_fontShader, m_promptText, promptPos, promptHeight);

    GL::Disable(GL_BLEND);
}

void GameOver::Shutdown()
{
    if (m_font) m_font->Shutdown();
    if (m_fontShader) m_fontShader.reset();
    if (m_colorShader) m_colorShader.reset();
    GL::DeleteVertexArrays(1, &m_quadVAO);
    GL::DeleteBuffers(1, &m_quadVBO);
}
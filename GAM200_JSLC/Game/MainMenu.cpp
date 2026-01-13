//MainMenu.cpp

#include "MainMenu.hpp"
#include "GameplayState.hpp"
#include "Font.hpp"

#include "../Engine/GameStateManager.hpp"
#include "../Engine/Input.hpp"
#include "../Engine/Engine.hpp"
#include "../OpenGL/Shader.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include "../Engine/Matrix.hpp" 

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
    // Clear background
    GL::ClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    GL::Clear(GL_COLOR_BUFFER_BIT);

    Math::Matrix projection = Math::Matrix::CreateOrtho(0.0f, GAME_WIDTH, 0.0f, GAME_HEIGHT, -1.0f, 1.0f);

    m_shapeShader->use();
    m_shapeShader->setMat4("projection", projection);

    GL::BindVertexArray(m_shapeVAO);

    // Lambda: DrawRect
    auto DrawRect = [&](float x, float y, float w, float h, float r, float g, float b) {
        Math::Matrix model = Math::Matrix::CreateTranslation({ x, y }) * Math::Matrix::CreateScale({ w, h });
        m_shapeShader->setMat4("model", model);
        m_shapeShader->setVec3("objectColor", r, g, b);
        GL::DrawArrays(GL_TRIANGLES, 0, 6);
    };

    // Lambda: DrawLetter
    auto DrawLetter = [&](char letter, float x, float y, float size, float r, float g, float b) {
        float t = size * 0.15f;
        float h = size; 
        float w = size * 0.7f;

        switch (letter) {
        case 'N':
            // Left leg
            DrawRect(x, y, t, h, r, g, b);
            // Right leg
            DrawRect(x + w - t, y, t, h, r, g, b);
            // Diagonal parts
            DrawRect(x + t, y + h * 0.6f, t, h * 0.2f, r, g, b);
            DrawRect(x + t * 1.5f, y + h * 0.4f, t, h * 0.2f, r, g, b);
            DrawRect(x + t * 2.5f, y + h * 0.2f, t * 0.5f, h * 0.2f, r, g, b);
            break;
        case 'o':
            DrawRect(x, y, w, t, r, g, b);          // Bottom
            DrawRect(x, y + h - t, w, t, r, g, b);  // Top
            DrawRect(x, y, t, h, r, g, b);          // Left
            DrawRect(x + w - t, y, t, h, r, g, b);  // Right
            break;
        case '.':
            DrawRect(x + t, y, t * 1.5f, t * 1.5f, r, g, b); // Dot
            break;
        case '9':
            DrawRect(x, y + h - t, w, t, r, g, b);      // Top
            DrawRect(x, y + h / 2, t, h / 2, r, g, b);  // Left Top
            DrawRect(x + w - t, y, t, h, r, g, b);      // Right
            DrawRect(x, y + h / 2, w, t, r, g, b);      // Middle
            DrawRect(x, y, w, t, r, g, b);              // Bottom
            break;
        case ' ': 
            // Space
            break;
        }
    };

    std::string title = "No. 99";

    // Variables for Text Animation
    float fontSize = 150.0f;
    float spacing = fontSize * 0.8f;
    float startX = (GAME_WIDTH - (title.length() * spacing)) / 2.0f;
    float startY = GAME_HEIGHT / 2.0f + 100.0f;

    // Glitch Effect
    bool isGlitch = (rand() % 100) < 5; // 5% chance
    float glitchX = 0.0f;
    float glitchY = 0.0f;

    if (isGlitch)
    {
        glitchX = (float)(rand() % 20 - 10);
        glitchY = (float)(rand() % 20 - 10);
    }

    // Shadow Layer (Dark Pink)
    for (size_t i = 0; i < title.length(); ++i)
    {
        float x = startX + i * spacing + glitchX;
        float y = startY + glitchY;
        DrawLetter(title[i], x + 8.0f, y - 8.0f, fontSize, 1.0f, 0.0f, 1.0f);
    }

    // Main Text Layer (Cyan)
    for (size_t i = 0; i < title.length(); ++i)
    {
        float x = startX + i * spacing;
        float y = startY;
        DrawLetter(title[i], x, y, fontSize, 0.0f, 1.0f, 1.0f);
    }

    GL::BindVertexArray(0);

    // Blending for Font
    GL::Enable(GL_BLEND);
    GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_fontShader->use();
    m_fontShader->setMat4("projection", projection);

    float pulse = (static_cast<float>(std::sin(m_glitchTimer * 3.0f)) + 1.0f) * 0.5f;
    float alpha = pulse * 0.8f + 0.2f;
    m_fontShader->setFloat("alpha", alpha);

    float promptHeight = 40.0f;
    // Check if font is valid before accessing members
    if(m_font) 
    {
        float promptWidth = m_promptText.width * (promptHeight / m_font->m_fontHeight);
        Math::Vec2 promptPos = {
            (GAME_WIDTH - promptWidth) / 2.0f,
            (GAME_HEIGHT / 2.0f) - 300.0f
        };
        m_font->DrawBakedText(*m_fontShader, m_promptText, promptPos, promptHeight);
    }

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
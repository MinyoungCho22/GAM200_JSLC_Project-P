// GameOver.cpp

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

// Logical screen dimensions
constexpr float GAME_WIDTH = 1920.0f;
constexpr float GAME_HEIGHT = 1080.0f;

GameOver::GameOver(GameStateManager& gsm_ref, bool& isGameOverFlag)
    : gsm(gsm_ref), m_isGameOverFlag(isGameOverFlag) {
    m_glitchTimer = 0.0;
}

void GameOver::Initialize()
{
    // Initialize shaders and font system
    m_fontShader = std::make_unique<Shader>("OpenGL/shaders/simple.vert", "OpenGL/shaders/simple.frag");
    m_colorShader = std::make_unique<Shader>("OpenGL/shaders/solid_color.vert", "OpenGL/shaders/solid_color.frag");

    m_font = std::make_unique<Font>();
    m_font->Initialize("Asset/fonts/Font_Outlined.png");

    // Pre-bake the prompt text to an OpenGL texture
    m_promptText = m_font->PrintToTexture(*m_fontShader, "Press ENTER to return to Main Menu");

    // Vertex data for a simple unit quad (Triangle strip)
    std::vector<float> vertices = {
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f
    };

    // Setup OpenGL buffers for procedurally drawing letters
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

    // Return to main menu on Enter key
    if (gsm.GetEngine().GetInput().IsKeyTriggered(Input::Key::Enter))
    {
        m_isExiting = true;
        m_isGameOverFlag = true; // Signal the engine that the game session is over
        gsm.PopState();
        return;
    }
}

void GameOver::Draw()
{
    // Use fixed orthographic projection for UI
    Math::Matrix projection = Math::Matrix::CreateOrtho(0.0f, GAME_WIDTH, 0.0f, GAME_HEIGHT, -1.0f, 1.0f);

    GL::Enable(GL_BLEND);
    GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_colorShader->use();
    m_colorShader->setMat4("projection", projection);

    GL::BindVertexArray(m_quadVAO);

    // Lambda function to draw a colored rectangle using the unit quad
    auto DrawRect = [&](float x, float y, float w, float h, float r, float g, float b) {
        Math::Matrix model = Math::Matrix::CreateTranslation({ x, y }) * Math::Matrix::CreateScale({ w, h });
        m_colorShader->setMat4("model", model);
        m_colorShader->setVec3("objectColor", r, g, b);
        GL::DrawArrays(GL_TRIANGLES, 0, 6);
    };

    // Lambda function to procedurally "draw" letters using blocky rectangles
    auto DrawLetter = [&](char letter, float x, float y, float size, float r, float g, float b) {
        float t = size * 0.15f; // Thickness
        float h = size;         // Height
        float w = size * 0.7f;  // Width

        switch (letter) {
        case 'T':
            DrawRect(x, y + h - t, w, t, r, g, b);           // Top bar
            DrawRect(x + w / 2 - t / 2, y, t, h, r, g, b);    // Center pillar
            break;
        case 'E':
            DrawRect(x, y, t, h, r, g, b);                   // Left pillar
            DrawRect(x, y + h - t, w, t, r, g, b);           // Top bar
            DrawRect(x, y + h * 0.5f - t * 0.5f, w * 0.8f, t, r, g, b); // Middle bar
            DrawRect(x, y, w, t, r, g, b);                   // Bottom bar
            break;
        case 'R':
            DrawRect(x, y, t, h, r, g, b);                   // Left pillar
            DrawRect(x, y + h - t, w, t, r, g, b);           // Top bar
            DrawRect(x + w - t, y + h * 0.5f, t, h * 0.5f, r, g, b); // Right curve (top half)
            DrawRect(x, y + h * 0.5f, w, t, r, g, b);        // Middle bar
            DrawRect(x + w - 2.0f * t, y + h * 0.25f, t, h * 0.25f, r, g, b); // Leg upper
            DrawRect(x + w - t, y, t, h * 0.25f, r, g, b);   // Leg lower
            break;
        case 'M':
            DrawRect(x, y, t, h, r, g, b);                   // Left pillar
            DrawRect(x + w - t, y, t, h, r, g, b);           // Right pillar
            DrawRect(x + t, y + h - t, t, t, r, g, b);       // V-shape left
            DrawRect(x + w - 2 * t, y + h - t, t, t, r, g, b); // V-shape right
            DrawRect(x + 2 * t, y + h - 2 * t, w - 4 * t, t, r, g, b); // V-shape bottom
            break;
        case 'I':
            DrawRect(x + w * 0.5f - t * 0.5f, y, t, h, r, g, b); // Center pillar
            DrawRect(x, y + h - t, w, t, r, g, b);           // Top bar
            DrawRect(x, y, w, t, r, g, b);                   // Bottom bar
            break;
        case 'N':
            DrawRect(x, y, t, h, r, g, b);                   // Left pillar
            DrawRect(x + w - t, y, t, h, r, g, b);           // Right pillar
            DrawRect(x + t, y + h * 0.6f, t, h * 0.2f, r, g, b); // Diagonal segments
            DrawRect(x + t * 2.0f, y + h * 0.3f, t, h * 0.2f, r, g, b);
            break;
        case 'A':
            DrawRect(x, y, t, h, r, g, b);                   // Left pillar
            DrawRect(x + w - t, y, t, h, r, g, b);           // Right pillar
            DrawRect(x, y + h - t, w, t, r, g, b);           // Top bar
            DrawRect(x, y + h * 0.5f, w, t, r, g, b);        // Middle bar
            break;
        case 'D':
            DrawRect(x, y, t, h, r, g, b);                   // Left pillar
            DrawRect(x, y + h - t, w - t, t, r, g, b);       // Top bar
            DrawRect(x, y, w - t, t, r, g, b);               // Bottom bar
            DrawRect(x + w - t, y + t, t, h - 2 * t, r, g, b); // Right closing bar
            break;
        }
    };

    std::string title = "TERMINATED";
    float fontSize = 160.0f;
    float spacing = fontSize * 0.8f;
    float startX = (GAME_WIDTH - (title.length() * spacing)) / 2.0f;
    float startY = GAME_HEIGHT / 2.0f - 30.0f;

    // Random glitch effect calculation
    bool isGlitch = (rand() % 100) < 5;
    float glitchX = 0.0f;
    float glitchY = 0.0f;
    if (isGlitch) {
        glitchX = (float)(rand() % 30 - 15);
        glitchY = (float)(rand() % 30 - 15);
    }

    // Render "Shadow" text layer (Magenta glitch)
    for (size_t i = 0; i < title.length(); ++i) {
        float x = startX + i * spacing + glitchX + 8.0f;
        float y = startY + glitchY - 8.0f;
        DrawLetter(title[i], x, y, fontSize, 1.0f, 0.0f, 1.0f);
    }

    // Render "Main" text layer (Cyan)
    for (size_t i = 0; i < title.length(); ++i) {
        float x = startX + i * spacing;
        float y = startY;
        DrawLetter(title[i], x, y, fontSize, 0.0f, 1.0f, 1.0f);
    }

    GL::BindVertexArray(0);

    // Render flashing prompt text
    m_fontShader->use();
    m_fontShader->setMat4("projection", projection);

    // Sinusoidal pulsing for the prompt alpha
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
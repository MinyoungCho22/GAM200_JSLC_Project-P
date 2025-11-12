#include "GameOver.hpp"
#include "MainMenu.hpp"
#include "../Engine/GameStateManager.hpp"
#include "../Engine/Engine.hpp"
#include "../OpenGL/Shader.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include "../Engine/Matrix.hpp"
#include <vector>

constexpr float GAME_WIDTH = 1920.0f;
constexpr float GAME_HEIGHT = 1080.0f;

GameOver::GameOver(GameStateManager& gsm_ref, bool& isGameOverFlag)
    : gsm(gsm_ref), m_isGameOverFlag(isGameOverFlag) {
}

void GameOver::Initialize()
{
    m_fontShader = std::make_unique<Shader>("OpenGL/shaders/simple.vert", "OpenGL/shaders/simple.frag");
    m_colorShader = std::make_unique<Shader>("OpenGL/shaders/solid_color.vert", "OpenGL/shaders/solid_color.frag");
    m_font = std::make_unique<Font>();
    m_font->Initialize("Asset/fonts/Font_Outlined.png");

    m_gameOverText = m_font->PrintToTexture(*m_fontShader, "They caught me! Escape failed!");
    m_promptText = m_font->PrintToTexture(*m_fontShader, "Press ENTER to return to Main Menu");

    std::vector<float> vertices = {
         -0.5f,  0.5f,
          0.5f,  0.5f,
         -0.5f, -0.5f,
          0.5f,  0.5f,
          0.5f, -0.5f,
         -0.5f, -0.5f
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

    Math::Matrix dimModel = Math::Matrix::CreateTranslation({ GAME_WIDTH / 2.0f, GAME_HEIGHT / 2.0f }) *
        Math::Matrix::CreateScale({ GAME_WIDTH, GAME_HEIGHT });
    m_colorShader->setMat4("model", dimModel);
    m_colorShader->setVec4("colorMultiplier", 0.0f, 0.0f, 0.0f, 0.7f);

    GL::BindVertexArray(m_quadVAO);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);
    GL::BindVertexArray(0);

    m_fontShader->use();
    m_fontShader->setMat4("projection", projection);

    float textHeight = 80.0f;
    float textWidth = m_gameOverText.width * (textHeight / m_font->m_fontHeight);
    Math::Vec2 textPos = { (GAME_WIDTH - textWidth) / 2.0f, (GAME_HEIGHT / 2.0f) + 50.0f };
    m_font->DrawBakedText(*m_fontShader, m_gameOverText, textPos, textHeight);

    float promptHeight = 40.0f;
    float promptWidth = m_promptText.width * (promptHeight / m_font->m_fontHeight);
    Math::Vec2 promptPos = { (GAME_WIDTH - promptWidth) / 2.0f, (GAME_HEIGHT / 2.0f) - 50.0f };
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
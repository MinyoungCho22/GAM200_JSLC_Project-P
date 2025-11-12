//MainMenu.cpp

#include "MainMenu.hpp"
#include "../Engine/GameStateManager.hpp"
#include "../Engine/Input.hpp"
#include "../Engine/Engine.hpp"
#include "../OpenGL/Shader.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include "../Engine/Matrix.hpp" 
#include "GameplayState.hpp" 

constexpr float GAME_WIDTH = 1920.0f;
constexpr float GAME_HEIGHT = 1080.0f;

MainMenu::MainMenu(GameStateManager& gsm_ref) : gsm(gsm_ref) {}

void MainMenu::Initialize()
{
    m_fontShader = std::make_unique<Shader>("OpenGL/shaders/simple.vert", "OpenGL/shaders/simple.frag");
    m_font = std::make_unique<Font>();
    m_font->Initialize("Asset/fonts/Font_Outlined.png");

    m_promptText = m_font->PrintToTexture(*m_fontShader, "Press ENTER to help the protagonist!");
}

void MainMenu::Update(double dt)
{
    if (gsm.GetEngine().GetInput().IsKeyTriggered(Input::Key::Enter))
    {
        gsm.ChangeState(std::make_unique<GameplayState>(gsm));
    }
}

void MainMenu::Draw()
{
    GL::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    GL::Clear(GL_COLOR_BUFFER_BIT);

    Math::Matrix projection = Math::Matrix::CreateOrtho(0.0f, GAME_WIDTH, 0.0f, GAME_HEIGHT, -1.0f, 1.0f);

    GL::Enable(GL_BLEND);
    GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_fontShader->use();
    m_fontShader->setMat4("projection", projection);

    float textHeight = 60.0f;
    float textWidth = m_promptText.width * (textHeight / m_font->m_fontHeight);
    Math::Vec2 textPos = {
        (GAME_WIDTH - textWidth) / 2.0f,
        (GAME_HEIGHT / 2.0f)
    };

    m_font->DrawBakedText(*m_fontShader, m_promptText, textPos, textHeight);

    GL::Disable(GL_BLEND);
}

void MainMenu::Shutdown()
{
    if (m_font) m_font->Shutdown();
    if (m_fontShader) m_fontShader.reset();
}
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

    // 타이틀과 안내 문구 텍스처
    m_titleText = m_font->PrintToTexture(*m_fontShader, "Blue Heart");
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
    m_fontShader->setFloat("alpha", 1.0f);

    // 타이틀 (Blue Heart) - 화면 중앙보다 조금 위
    float titleHeight = 150.0f; // 타이틀 크기를 키움
    float titleWidth = m_titleText.width * (titleHeight / m_font->m_fontHeight);
    Math::Vec2 titlePos = {
        (GAME_WIDTH - titleWidth) / 2.0f,
        (GAME_HEIGHT / 2.0f) + 150.0f // Y축 +방향으로 올려서 배치
    };
    m_font->DrawBakedText(*m_fontShader, m_titleText, titlePos, titleHeight);

    // 안내 문구 (Press ENTER...) - 화면 아래쪽으로 내림
    float promptHeight = 50.0f; // 안내 문구는 작게
    float promptWidth = m_promptText.width * (promptHeight / m_font->m_fontHeight);
    Math::Vec2 promptPos = {
        (GAME_WIDTH - promptWidth) / 2.0f,
        (GAME_HEIGHT / 2.0f) - 300.0f // Y축 -방향으로 내려서 배치
    };
    m_font->DrawBakedText(*m_fontShader, m_promptText, promptPos, promptHeight);

    GL::Disable(GL_BLEND);
}

void MainMenu::Shutdown()
{
    if (m_font) m_font->Shutdown();
    if (m_fontShader) m_fontShader.reset();
}
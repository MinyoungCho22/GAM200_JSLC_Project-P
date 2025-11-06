#include "Setting.hpp"
#include "../Engine/GameStateManager.hpp"
#include "../Engine/Engine.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/Logger.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include "Font.hpp"

SettingState::SettingState(GameStateManager& gsm_ref)
    : gsm(gsm_ref), m_currentSelection(MenuOption::Setting), m_overlayVAO(0), m_overlayVBO(0) {
}

void SettingState::Initialize()
{
    Logger::Instance().Log(Logger::Severity::Info, "SettingState Initialize");

    // 폰트와 컬러 셰이더 로드
    m_font = std::make_unique<Font>();
    m_font->Initialize("Asset/fonts/Font_Outlined.png", 16, 8);
    m_colorShader = std::make_unique<Shader>("OpenGL/shaders/solid_color.vert", "OpenGL/shaders/solid_color.frag");

    // 화면을 덮을 반투명 사각형을 위한 VAO/VBO 생성
    float vertices[] = {
        // positions (0.0 ~ 1.0 범위)
        0.0f, 1.0f,
        1.0f, 0.0f,
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
        1.0f, 0.0f
    };

    GL::GenVertexArrays(1, &m_overlayVAO);
    GL::GenBuffers(1, &m_overlayVBO);
    GL::BindVertexArray(m_overlayVAO);
    GL::BindBuffer(GL_ARRAY_BUFFER, m_overlayVBO);
    GL::BufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    GL::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    GL::EnableVertexAttribArray(0);
    GL::BindVertexArray(0);
}

void SettingState::Update(double dt)
{
    Engine& engine = gsm.GetEngine();
    auto& input = engine.GetInput();

    // ESC 키를 누르면 상태를 Pop (GameplayState로 돌아감)
    if (input.IsKeyTriggered(Input::Key::Escape))
    {
        gsm.PopState();
        return;
    }

    // W (위) / S (아래) 키로 메뉴 선택
    if (input.IsKeyTriggered(Input::Key::W))
    {
        m_currentSelection = MenuOption::Setting;
    }
    else if (input.IsKeyTriggered(Input::Key::S))
    {
        m_currentSelection = MenuOption::Exit;
    }

    // Enter 키로 선택 실행
    if (input.IsKeyTriggered(Input::Key::Enter) || input.IsKeyTriggered(Input::Key::Space))
    {
        if (m_currentSelection == MenuOption::Setting)
        {
            // TODO: 이곳에 실제 설정 화면으로 넘어가는 로직 구현
            Logger::Instance().Log(Logger::Severity::Event, "Setting option selected (not implemented)");
            gsm.PopState(); // 지금은 그냥 닫기
        }
        else if (m_currentSelection == MenuOption::Exit)
        {
            engine.RequestShutdown();
        }
    }
}

void SettingState::Draw()
{
    // GameplayState가 이미 화면을 그렸으므로, Clear()를 호출하지 않습니다.

    Engine& engine = gsm.GetEngine();
    float screenWidth = static_cast<float>(engine.GetWidth());
    float screenHeight = static_cast<float>(engine.GetHeight());

    Math::Matrix projection = Math::Matrix::CreateOrtho(0.0f, screenWidth, 0.0f, screenHeight, -1.0f, 1.0f);

    // 1. 반투명 검은색 오버레이 그리기
    m_colorShader->use();
    m_colorShader->setMat4("projection", projection);

    Math::Matrix overlayModel = Math::Matrix::CreateTranslation({ screenWidth / 2.0f, screenHeight / 2.0f }) * Math::Matrix::CreateScale({ screenWidth, screenHeight });
    m_colorShader->setMat4("model", overlayModel);
    m_colorShader->setVec4("objectColor", 0.0f, 0.0f, 0.0f, 0.7f); // 70% 투명도의 검은색

    // 텍스처 셰이더를 사용하지 않으므로, 텍스처 바인딩 해제 (중요)
    GL::BindTexture(GL_TEXTURE_2D, 0);
    GL::BindVertexArray(m_overlayVAO);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);

    // 2. 텍스트 그리기 (텍스처 셰이더 사용)
    gsm.GetEngine().GetTextureShader().use(); // GameplayState의 텍스처 셰이더 재사용
    gsm.GetEngine().GetTextureShader().setMat4("projection", projection);

    Math::Vec2 settingPos = { screenWidth / 2.0f - 150.f, screenHeight / 2.0f + 50.f };
    Math::Vec2 exitPos = { screenWidth / 2.0f - 150.f, screenHeight / 2.0f - 50.f };
    float size = 64.0f;

 

    std::string settingText = "Setting";
    std::string exitText = "Exit";

    if (m_currentSelection == MenuOption::Setting)
    {
        settingText = "> Setting <";
    }
    else
    {
        exitText = "> Exit <";
    }

    m_font->DrawText(gsm.GetEngine().GetTextureShader(), settingText, settingPos, size);
    m_font->DrawText(gsm.GetEngine().GetTextureShader(), exitText, exitPos, size);
}

void SettingState::Shutdown()
{
    m_font->Shutdown();
    GL::DeleteVertexArrays(1, &m_overlayVAO);
    GL::DeleteBuffers(1, &m_overlayVBO);
    Logger::Instance().Log(Logger::Severity::Info, "SettingState Shutdown");
}
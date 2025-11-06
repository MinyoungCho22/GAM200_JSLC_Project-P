#include "Setting.hpp"
#include "../Engine/GameStateManager.hpp"
#include "../Engine/Engine.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/Logger.hpp"
#include "../OpenGL/GLWrapper.hpp"

#include <string>
#include <sstream>

SettingState::SettingState(GameStateManager& gsm_ref)
    : gsm(gsm_ref), m_mainSelection(MainOption::Resume), m_overlayVAO(0), m_overlayVBO(0) {
    m_currentPage = MenuPage::Main;
    m_displaySelection = 0;
}

void SettingState::Initialize()
{
    Logger::Instance().Log(Logger::Severity::Info, "SettingState Initialize");

    m_font = std::make_unique<Font>();
    m_font->Initialize("Asset/fonts/Font_Outlined.png");

    m_colorShader = std::make_unique<Shader>("OpenGL/shaders/solid_color.vert", "OpenGL/shaders/solid_color.frag");

    m_fontShader = std::make_unique<Shader>("OpenGL/shaders/simple.vert", "OpenGL/shaders/simple.frag");
    m_fontShader->use();
    m_fontShader->setInt("ourTexture", 0);

    // --- 텍스트 미리 베이킹 ---

    // 1. 메인 메뉴 텍스트
    m_resumeText = m_font->PrintToTexture(*m_fontShader, "Setting");
    m_resumeSelectedText = m_font->PrintToTexture(*m_fontShader, "> Setting <");
    m_exitText = m_font->PrintToTexture(*m_fontShader, "Exit");
    m_exitSelectedText = m_font->PrintToTexture(*m_fontShader, "> Exit <");

    // 2. 디스플레이 메뉴 텍스트
    Math::ivec2 recRes = gsm.GetEngine().GetRecommendedResolution();
    std::stringstream ss;
    ss << recRes.x << " x " << recRes.y << " (Recommended)";

    m_resRecommendedText = m_font->PrintToTexture(*m_fontShader, ss.str());

    //삭제 m_res1600Text = m_font->PrintToTexture(*m_fontShader, "1600 x 900");


    // --- 오버레이 VAO/VBO 생성 ---
    float vertices[] = {
        0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f
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

    if (m_currentPage == MenuPage::Main)
    {
        // --- 메인 메뉴 (Setting / Exit) ---
        if (input.IsKeyTriggered(Input::Key::Escape))
        {
            gsm.PopState();
            return;
        }
        if (input.IsKeyTriggered(Input::Key::W))
        {
            m_mainSelection = MainOption::Resume;
        }
        else if (input.IsKeyTriggered(Input::Key::S))
        {
            m_mainSelection = MainOption::Exit;
        }

        if (input.IsKeyTriggered(Input::Key::Enter) || input.IsKeyTriggered(Input::Key::Space))
        {
            if (m_mainSelection == MainOption::Resume)
            {
                m_currentPage = MenuPage::Display;
                m_displaySelection = 0;
            }
            else if (m_mainSelection == MainOption::Exit)
            {
                engine.RequestShutdown();
            }
        }
    }
    else if (m_currentPage == MenuPage::Display)
    {
        // --- 디스플레이 설정 메뉴 (해상도 선택) ---
        if (input.IsKeyTriggered(Input::Key::Escape))
        {
            m_currentPage = MenuPage::Main; // 메인 메뉴로 돌아가기
            return;
        }

        //
        /*
        if (input.IsKeyTriggered(Input::Key::W))
        {
            m_displaySelection = 0;
        }
        else if (input.IsKeyTriggered(Input::Key::S))
        {
            m_displaySelection = 1;
        }
        */

        if (input.IsKeyTriggered(Input::Key::Enter) || input.IsKeyTriggered(Input::Key::Space))
        {

            if (m_displaySelection == 0)
            {
                Math::ivec2 res = gsm.GetEngine().GetRecommendedResolution();
                Logger::Instance().Log(Logger::Severity::Event, "Apply Resolution: %d x %d", res.x, res.y);
                gsm.GetEngine().SetResolution(res.x, res.y);
            }
        }
    }
}

void SettingState::Draw()
{
    Engine& engine = gsm.GetEngine();
    float screenWidth = static_cast<float>(engine.GetWidth());
    float screenHeight = static_cast<float>(engine.GetHeight());

    GL::Viewport(0, 0, engine.GetWidth(), engine.GetHeight());
    Math::Matrix projection = Math::Matrix::CreateOrtho(0.0f, screenWidth, 0.0f, screenHeight, -1.0f, 1.0f);

    // 1. 반투명 검은색 오버레이 그리기 (공통)
    m_colorShader->use();
    m_colorShader->setMat4("projection", projection);
    Math::Matrix overlayModel = Math::Matrix::CreateTranslation({ screenWidth / 2.0f, screenHeight / 2.0f }) * Math::Matrix::CreateScale({ screenWidth, screenHeight });
    m_colorShader->setMat4("model", overlayModel);
    m_colorShader->setVec4("objectColor", 0.0f, 0.0f, 0.0f, 0.7f);

    GL::Enable(GL_BLEND);
    GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    GL::BindTexture(GL_TEXTURE_2D, 0);
    GL::BindVertexArray(m_overlayVAO);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);
    GL::BindVertexArray(0);

    // 2. 텍스트 그리기 (페이지에 따라 분기)
    m_fontShader->use();
    m_fontShader->setMat4("projection", projection);

    float size = 64.0f;

    if (m_currentPage == MenuPage::Main)
    {
        // --- 메인 메뉴 그리기 ---
        Math::Vec2 settingPos = { screenWidth / 2.0f - 150.f, screenHeight / 2.0f + 50.f };
        Math::Vec2 exitPos = { screenWidth / 2.0f - 150.f, screenHeight / 2.0f - 50.f };

        if (m_mainSelection == MainOption::Resume) // 'Setting'
        {
            m_font->DrawBakedText(*m_fontShader, m_resumeSelectedText, settingPos, size);
            m_font->DrawBakedText(*m_fontShader, m_exitText, exitPos, size);
        }
        else // m_mainSelection == MainOption::Exit
        {
            m_font->DrawBakedText(*m_fontShader, m_resumeText, settingPos, size);
            m_font->DrawBakedText(*m_fontShader, m_exitSelectedText, exitPos, size);
        }
    }
    else if (m_currentPage == MenuPage::Display)
    {
        // --- 디스플레이 메뉴 그리기 ---
        Math::Vec2 resRecPos = { screenWidth / 2.0f - 400.f, screenHeight / 2.0f + 50.f };


        if (m_displaySelection == 0)
        {
            // 항상 Recommended만 선택된 상태로 표시
            std::string selectedStr = "> " + m_resRecommendedText.text + " <";
            CachedTextureInfo selectedTex = m_font->PrintToTexture(*m_fontShader, selectedStr);

            m_font->DrawBakedText(*m_fontShader, selectedTex, resRecPos, size);
        }
    }
}

void SettingState::Shutdown()
{

    GL::DeleteVertexArrays(1, &m_overlayVAO);
    GL::DeleteBuffers(1, &m_overlayVBO);
    Logger::Instance().Log(Logger::Severity::Info, "SettingState Shutdown");
}
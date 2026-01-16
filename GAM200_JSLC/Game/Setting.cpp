// Setting.cpp

#include "Setting.hpp"
#include "../Engine/GameStateManager.hpp"
#include "../Engine/Engine.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/Logger.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include <GLFW/glfw3.h>

#include <string>
#include <sstream>

// Fixed logical resolution for the UI system
constexpr float GAME_WIDTH = 1920.0f;
constexpr float GAME_HEIGHT = 1080.0f;

SettingState::SettingState(GameStateManager& gsm_ref)
    : gsm(gsm_ref), m_mainSelection(MainOption::Resume), m_overlayVAO(0), m_overlayVBO(0) {
    m_currentPage = MenuPage::Main;
    m_displaySelection = 0;
}

void SettingState::Initialize()
{
    Logger::Instance().Log(Logger::Severity::Info, "SettingState Initialize");

    // Initialize font and shaders
    m_font = std::make_unique<Font>();
    m_font->Initialize("Asset/fonts/Font_Outlined.png");

    m_colorShader = std::make_unique<Shader>("OpenGL/shaders/solid_color.vert", "OpenGL/shaders/solid_color.frag");
    m_fontShader = std::make_unique<Shader>("OpenGL/shaders/simple.vert", "OpenGL/shaders/simple.frag");
    
    m_fontShader->use();
    m_fontShader->setInt("ourTexture", 0);

    // --- Pre-bake text textures ---
    // This converts strings to OpenGL textures once to avoid per-frame text rendering overhead.

    // 1. Main menu options
    m_resumeText = m_font->PrintToTexture(*m_fontShader, "Setting");
    m_resumeSelectedText = m_font->PrintToTexture(*m_fontShader, "> Setting <");
    m_exitText = m_font->PrintToTexture(*m_fontShader, "Exit");
    m_exitSelectedText = m_font->PrintToTexture(*m_fontShader, "> Exit <");

    // 2. Display menu options (dynamic based on system/monitor)
    Math::ivec2 recRes = gsm.GetEngine().GetRecommendedResolution();
    std::stringstream ss;
    ss << recRes.x << " x " << recRes.y << " (Recommended)";
    m_resRecommendedText = m_font->PrintToTexture(*m_fontShader, ss.str());

    // --- Initialize Overlay Geometry ---
    // A simple full-screen quad used to dim the background.
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
        // --- Navigation for Main Settings Page ---
        if (input.IsKeyTriggered(Input::Key::Escape))
        {
            gsm.PopState(); // Return to previous state (Game or MainMenu)
            return;
        }

        if (input.IsKeyTriggered(Input::Key::W)) { m_mainSelection = MainOption::Resume; }
        else if (input.IsKeyTriggered(Input::Key::S)) { m_mainSelection = MainOption::Exit; }

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
        // --- Navigation for Display Settings Page ---
        if (input.IsKeyTriggered(Input::Key::Escape))
        {
            m_currentPage = MenuPage::Main;
            return;
        }

        if (input.IsKeyTriggered(Input::Key::Enter) || input.IsKeyTriggered(Input::Key::Space))
        {
            if (m_displaySelection == 0)
            {
                // Apply the recommended resolution to the window
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

    // --- Viewport & Aspect Ratio Calculation ---
    // Calculates Letterbox (horizontal bars) or Pillarbox (vertical bars)
    int windowWidth, windowHeight;
    glfwGetFramebufferSize(engine.GetWindow(), &windowWidth, &windowHeight);

    float windowAspect = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);
    float gameAspect = GAME_WIDTH / GAME_HEIGHT;

    int viewportX = 0, viewportY = 0;
    int viewportWidth = windowWidth, viewportHeight = windowHeight;

    if (windowAspect > gameAspect) // Pillarbox
    {
        viewportWidth = static_cast<int>(windowHeight * gameAspect);
        viewportX = (windowWidth - viewportWidth) / 2;
    }
    else if (windowAspect < gameAspect) // Letterbox
    {
        viewportHeight = static_cast<int>(windowWidth / gameAspect);
        viewportY = (windowHeight - viewportHeight) / 2;
    }

    GL::Viewport(viewportX, viewportY, viewportWidth, viewportHeight);

    // Create orthographic projection based on fixed 1920x1080 UI resolution
    Math::Matrix projection = Math::Matrix::CreateOrtho(0.0f, GAME_WIDTH, 0.0f, GAME_HEIGHT, -1.0f, 1.0f);

    // 1. Render Semi-transparent Dimming Overlay
    m_colorShader->use();
    m_colorShader->setMat4("projection", projection);
    Math::Matrix overlayModel = Math::Matrix::CreateTranslation({ GAME_WIDTH / 2.0f, GAME_HEIGHT / 2.0f }) *
                                Math::Matrix::CreateScale({ GAME_WIDTH, GAME_HEIGHT });
    m_colorShader->setMat4("model", overlayModel);
    m_colorShader->setVec4("objectColor", 0.0f, 0.0f, 0.0f, 0.7f); // 70% opacity black

    GL::Enable(GL_BLEND);
    GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    GL::BindTexture(GL_TEXTURE_2D, 0);
    GL::BindVertexArray(m_overlayVAO);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);
    GL::BindVertexArray(0);

    // 2. Render Menu Text based on current page
    m_fontShader->use();
    m_fontShader->setMat4("projection", projection);

    float fontSize = 64.0f;

    if (m_currentPage == MenuPage::Main)
    {
        Math::Vec2 settingPos = { GAME_WIDTH / 2.0f - 150.f, GAME_HEIGHT / 2.0f + 50.f };
        Math::Vec2 exitPos = { GAME_WIDTH / 2.0f - 150.f, GAME_HEIGHT / 2.0f - 50.f };

        if (m_mainSelection == MainOption::Resume)
        {
            m_font->DrawBakedText(*m_fontShader, m_resumeSelectedText, settingPos, fontSize);
            m_font->DrawBakedText(*m_fontShader, m_exitText, exitPos, fontSize);
        }
        else
        {
            m_font->DrawBakedText(*m_fontShader, m_resumeText, settingPos, fontSize);
            m_font->DrawBakedText(*m_fontShader, m_exitSelectedText, exitPos, fontSize);
        }
    }
    else if (m_currentPage == MenuPage::Display)
    {
        Math::Vec2 resRecPos = { GAME_WIDTH / 2.0f - 400.f, GAME_HEIGHT / 2.0f + 50.f };

        if (m_displaySelection == 0)
        {
            // For the demo, always show the Recommended option as selected
            std::string selectedStr = "> " + m_resRecommendedText.text + " <";
            CachedTextureInfo selectedTex = m_font->PrintToTexture(*m_fontShader, selectedStr);
            m_font->DrawBakedText(*m_fontShader, selectedTex, resRecPos, fontSize);
        }
    }
}



void SettingState::Shutdown()
{
    // Release OpenGL resources
    GL::DeleteVertexArrays(1, &m_overlayVAO);
    GL::DeleteBuffers(1, &m_overlayVBO);
    Logger::Instance().Log(Logger::Severity::Info, "SettingState Shutdown");
}
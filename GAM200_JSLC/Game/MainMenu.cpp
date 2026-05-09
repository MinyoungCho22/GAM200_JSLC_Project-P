//MainMenu.cpp

#include "MainMenu.hpp"
#include "GameplayState.hpp"
#include "Setting.hpp"
#include "CreditsState.hpp"

#include "../Engine/GameStateManager.hpp"
#include "../Engine/Input.hpp"
#include "../Engine/Engine.hpp"
#include "../Engine/Logger.hpp"
#include "../OpenGL/Shader.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include "../Engine/Matrix.hpp"

#pragma warning(push, 0)
#include <stb_image.h>
#pragma warning(pop)

#include <algorithm>

constexpr float GAME_WIDTH  = 1920.0f;
constexpr float GAME_HEIGHT = 1080.0f;

// Y-center of each menu item (ortho y-up: 0=bottom, 1080=top)
// Order: Play, Settings, Credits, Exit
static constexpr float ITEM_Y[4]       = { 570.0f, 470.0f, 370.0f, 270.0f };
static constexpr float CLICK_HEIGHT    = 90.0f;
static constexpr float kFadeOutSecs    = 0.2f;

// ─────────────────────────────────────────────────────────────────────────────
static void LoadTex(const char* path, unsigned int& id, int& w, int& h)
{
    GL::GenTextures(1, &id);
    GL::BindTexture(GL_TEXTURE_2D, id);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int ch = 0;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* d = stbi_load(path, &w, &h, &ch, 0);
    if (d)
    {
        GLenum fmt = (ch == 4) ? GL_RGBA : GL_RGB;
        GL::TexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(fmt), w, h, 0, fmt, GL_UNSIGNED_BYTE, d);
        GL::GenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(d);
    }
    else
    {
        Logger::Instance().Log(Logger::Severity::Error,
            "MainMenu: failed to load texture '%s'", path);
        w = h = 1;
        unsigned char white[] = { 255, 255, 255, 255 };
        GL::TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    }
    GL::BindTexture(GL_TEXTURE_2D, 0);
}

// ─────────────────────────────────────────────────────────────────────────────
MainMenu::MainMenu(GameStateManager& gsm_ref) : gsm(gsm_ref) {}

void MainMenu::Initialize()
{
    auto& input = gsm.GetEngine().GetInput();

    m_selectedItem        = 0;
    m_isFadingOut         = false;
    m_fadeAlpha           = 0.0f;
    m_pendingAction       = -1;
    m_waitForEnterRelease = input.IsKeyPressed(Input::Key::Enter);
    m_waitForNavRelease   = input.IsKeyPressed(Input::Key::W)    ||
                            input.IsKeyPressed(Input::Key::S)    ||
                            input.IsKeyPressed(Input::Key::Up)   ||
                            input.IsKeyPressed(Input::Key::Down);

    // ── Texture shader ────────────────────────────────────────────────────
    m_texShader = std::make_unique<Shader>("OpenGL/Shaders/simple.vert",
                                           "OpenGL/Shaders/simple.frag");
    m_texShader->use();
    m_texShader->setInt("ourTexture", 0);
    m_texShader->setVec3("colorTint", 0.0f, 0.0f, 0.0f);
    m_texShader->setFloat("tintStrength", 0.0f);

    // ── Textured quad: bottom-left origin [0,1]×[0,1] with UV ────────────
    float verts[] = {
        0.0f, 0.0f,  0.0f, 0.0f,   // bottom-left
        1.0f, 0.0f,  1.0f, 0.0f,   // bottom-right
        0.0f, 1.0f,  0.0f, 1.0f,   // top-left

        1.0f, 0.0f,  1.0f, 0.0f,   // bottom-right
        1.0f, 1.0f,  1.0f, 1.0f,   // top-right
        0.0f, 1.0f,  0.0f, 1.0f,   // top-left
    };
    GL::GenVertexArrays(1, &m_texVAO);
    GL::GenBuffers(1, &m_texVBO);
    GL::BindVertexArray(m_texVAO);
    GL::BindBuffer(GL_ARRAY_BUFFER, m_texVBO);
    GL::BufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    GL::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    GL::EnableVertexAttribArray(0);
    GL::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                            (void*)(2 * sizeof(float)));
    GL::EnableVertexAttribArray(1);
    GL::BindVertexArray(0);

    // ── Textures ──────────────────────────────────────────────────────────
    LoadTex("Asset/MainMenu.png",       m_bgTexID,    m_bgTexW,    m_bgTexH);
    LoadTex("Asset/MainMenu_Click.png", m_clickTexID, m_clickTexW, m_clickTexH);

    // ── Fade quad (centered unit quad, solid_color shader) ────────────────
    if (m_fadeVAO == 0)
    {
        m_fadeShader = std::make_unique<Shader>("OpenGL/Shaders/solid_color.vert",
                                                "OpenGL/Shaders/solid_color.frag");
        float fadeVerts[] = {
            -0.5f,  0.5f,   0.5f,  0.5f,  -0.5f, -0.5f,
             0.5f,  0.5f,   0.5f, -0.5f,  -0.5f, -0.5f
        };
        GL::GenVertexArrays(1, &m_fadeVAO);
        GL::GenBuffers(1, &m_fadeVBO);
        GL::BindVertexArray(m_fadeVAO);
        GL::BindBuffer(GL_ARRAY_BUFFER, m_fadeVBO);
        GL::BufferData(GL_ARRAY_BUFFER, sizeof(fadeVerts), fadeVerts, GL_STATIC_DRAW);
        GL::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        GL::EnableVertexAttribArray(0);
        GL::BindVertexArray(0);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void MainMenu::Update(double dt)
{
    auto& input = gsm.GetEngine().GetInput();

    // Wait until Enter held from the previous state is released
    if (m_waitForEnterRelease)
    {
        if (!input.IsKeyPressed(Input::Key::Enter))
            m_waitForEnterRelease = false;
        return;
    }

    // Wait until navigation keys held on entry are released
    if (m_waitForNavRelease)
    {
        bool anyNav = input.IsKeyPressed(Input::Key::W)  ||
                      input.IsKeyPressed(Input::Key::S)  ||
                      input.IsKeyPressed(Input::Key::Up) ||
                      input.IsKeyPressed(Input::Key::Down);
        if (!anyNav)
            m_waitForNavRelease = false;
        return;
    }

    // Fade-out in progress → advance alpha, then execute pending action
    if (m_isFadingOut)
    {
        m_fadeAlpha = std::min(1.0f, m_fadeAlpha + static_cast<float>(dt) / kFadeOutSecs);
        if (m_fadeAlpha >= 1.0f)
        {
            int action      = m_pendingAction;
            m_isFadingOut   = false;
            m_pendingAction = -1;

            switch (action)
            {
            case 0: // Play
                Logger::Instance().Log(Logger::Severity::Event,
                    "MainMenu: starting game");
                gsm.ChangeState(std::make_unique<GameplayState>(gsm));
                return; // 'this' is destroyed by ChangeState

            case 1: // Settings
                m_fadeAlpha           = 0.0f;
                m_waitForEnterRelease = true;
                gsm.PushState(std::make_unique<SettingState>(gsm));
                return;

            case 2: // Credits
                m_fadeAlpha           = 0.0f;
                m_waitForEnterRelease = true;
                gsm.PushState(std::make_unique<CreditsState>(gsm));
                return;

            case 3: // Exit
#ifdef __EMSCRIPTEN__
                gsm.GetEngine().RequestReturnToSplash();
#else
                gsm.GetEngine().RequestShutdown();
#endif
                return;

            default:
                break;
            }
        }
        return;
    }

    // W / Up  → move selection up
    // S / Down → move selection down
    if (input.IsKeyTriggered(Input::Key::W) || input.IsKeyTriggered(Input::Key::Up))
        m_selectedItem = (m_selectedItem - 1 + 4) % 4;

    if (input.IsKeyTriggered(Input::Key::S) || input.IsKeyTriggered(Input::Key::Down))
        m_selectedItem = (m_selectedItem + 1) % 4;

    // Enter → confirm selected item with fade
    if (input.IsKeyTriggered(Input::Key::Enter))
    {
        m_isFadingOut   = true;
        m_pendingAction = m_selectedItem;
        Logger::Instance().Log(Logger::Severity::Event,
            "MainMenu: confirmed item %d", m_selectedItem);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void MainMenu::Draw()
{
    GL::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    GL::Clear(GL_COLOR_BUFFER_BIT);

    Math::Matrix proj = Math::Matrix::CreateOrtho(
        0.0f, GAME_WIDTH, 0.0f, GAME_HEIGHT, -1.0f, 1.0f);

    GL::Enable(GL_BLEND);
    GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // ── Textured draws ────────────────────────────────────────────────────
    m_texShader->use();
    m_texShader->setMat4("projection", proj);
    m_texShader->setBool("flipX", false);
    m_texShader->setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
    m_texShader->setFloat("alpha", 1.0f);

    GL::BindVertexArray(m_texVAO);
    GL::ActiveTexture(GL_TEXTURE0);

    // Background image (full screen)
    {
        Math::Matrix model = Math::Matrix::CreateTranslation({ 0.0f, 0.0f })
                           * Math::Matrix::CreateScale({ GAME_WIDTH, GAME_HEIGHT });
        m_texShader->setMat4("model", model);
        GL::BindTexture(GL_TEXTURE_2D, m_bgTexID);
        GL::DrawArrays(GL_TRIANGLES, 0, 6);
    }

    // Selection indicator (full width, centered on selected item)
    {
        float yCenter = ITEM_Y[m_selectedItem];
        float yBottom = yCenter - CLICK_HEIGHT * 0.5f;

        Math::Matrix model = Math::Matrix::CreateTranslation({ 0.0f, yBottom })
                           * Math::Matrix::CreateScale({ GAME_WIDTH, CLICK_HEIGHT });
        m_texShader->setMat4("model", model);
        GL::BindTexture(GL_TEXTURE_2D, m_clickTexID);
        GL::DrawArrays(GL_TRIANGLES, 0, 6);
    }

    GL::BindVertexArray(0);

    // ── Fade-out overlay ──────────────────────────────────────────────────
    if (m_fadeAlpha > 0.0f && m_fadeVAO != 0)
    {
        Math::Matrix fadeModel =
            Math::Matrix::CreateTranslation({ GAME_WIDTH * 0.5f, GAME_HEIGHT * 0.5f })
          * Math::Matrix::CreateScale({ GAME_WIDTH, GAME_HEIGHT });

        m_fadeShader->use();
        m_fadeShader->setMat4("projection", proj);
        m_fadeShader->setMat4("model", fadeModel);
        m_fadeShader->setVec3("objectColor", 0.0f, 0.0f, 0.0f);
        m_fadeShader->setFloat("uAlpha", m_fadeAlpha);

        GL::BindVertexArray(m_fadeVAO);
        GL::DrawArrays(GL_TRIANGLES, 0, 6);
        GL::BindVertexArray(0);
    }

    GL::Disable(GL_BLEND);
}

// ─────────────────────────────────────────────────────────────────────────────
void MainMenu::Shutdown()
{
    if (m_texShader) m_texShader.reset();
    if (m_texVAO != 0)
    {
        GL::DeleteVertexArrays(1, &m_texVAO);
        GL::DeleteBuffers(1, &m_texVBO);
        m_texVAO = 0;
        m_texVBO = 0;
    }

    if (m_bgTexID != 0)    { GL::DeleteTextures(1, &m_bgTexID);    m_bgTexID    = 0; }
    if (m_clickTexID != 0) { GL::DeleteTextures(1, &m_clickTexID); m_clickTexID = 0; }

    if (m_fadeShader) m_fadeShader.reset();
    if (m_fadeVAO != 0)
    {
        GL::DeleteVertexArrays(1, &m_fadeVAO);
        GL::DeleteBuffers(1, &m_fadeVBO);
        m_fadeVAO = 0;
        m_fadeVBO = 0;
    }
}

//CreditsState.cpp

#include "CreditsState.hpp"
#include "../Engine/GameStateManager.hpp"
#include "../Engine/Input.hpp"
#include "../Engine/Engine.hpp"
#include "../Engine/Logger.hpp"
#include "../Engine/Matrix.hpp"
#include "../OpenGL/Shader.hpp"
#include "../OpenGL/GLWrapper.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

constexpr float CW = 1920.0f;
constexpr float CH = 1080.0f;

// ─────────────────────────────────────────────────────────────────────────────
CreditsState::CreditsState(GameStateManager& gsm_ref) : gsm(gsm_ref) {}

void CreditsState::Initialize()
{
#ifdef __EMSCRIPTEN__
    EM_ASM({
        window.gameCreditsActive = true;
    });
#endif

    auto& input = gsm.GetEngine().GetInput();
    m_waitForRelease = input.IsKeyPressed(Input::Key::Enter) ||
                       input.IsKeyPressed(Input::Key::Escape);

    // ── Color shader + unit rect quad (centered) ──────────────────────────
    m_colorShader = std::make_unique<Shader>("OpenGL/Shaders/solid_color.vert",
                                             "OpenGL/Shaders/solid_color.frag");
    float rv[] = {
        -0.5f,  0.5f,   0.5f,  0.5f,  -0.5f, -0.5f,
         0.5f,  0.5f,   0.5f, -0.5f,  -0.5f, -0.5f
    };
    GL::GenVertexArrays(1, &m_rectVAO);
    GL::GenBuffers(1, &m_rectVBO);
    GL::BindVertexArray(m_rectVAO);
    GL::BindBuffer(GL_ARRAY_BUFFER, m_rectVBO);
    GL::BufferData(GL_ARRAY_BUFFER, sizeof(rv), rv, GL_STATIC_DRAW);
    GL::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    GL::EnableVertexAttribArray(0);
    GL::BindVertexArray(0);

    // ── Font ─────────────────────────────────────────────────────────────
    m_fontShader = std::make_unique<Shader>("OpenGL/Shaders/simple.vert",
                                            "OpenGL/Shaders/simple.frag");
    m_font = std::make_unique<Font>();
    m_font->Initialize("Asset/fonts/Font_Outlined.png");

    // ── Bake text lines ───────────────────────────────────────────────────
    // { text,  display size (height in game pixels) }
    const struct { const char* text; float size; } defs[] = {
        { "CREDITS",                        80.0f },
        { "",                                0.0f },
        { "Tech Lead",                      40.0f },
        { "Minyoung Cho",                   36.0f },
        { "",                                0.0f },
        { "QA & Art Lead",                  40.0f },
        { "Jisu Son",                        36.0f },
        { "",                                0.0f },
        { "Design Lead & Level Design Lead", 40.0f },
        { "Seoyoung Jung",                   36.0f },
        { "",                                0.0f },
        { "Producer",                        40.0f },
        { "Ryungjae Lee",                    36.0f },
        { "",                                0.0f },
        { "Special Thanks",                  40.0f },
        { "DigiPen Institute of Technology", 34.0f },
        { "",                                0.0f },
        { "ESC / ENTER  to return",          34.0f },
    };

    m_lines.clear();
    for (auto& d : defs)
    {
        if (d.size == 0.0f)
        {
            m_lines.push_back({ {0, 0, 0}, 0.0f });
        }
        else
        {
            CachedTextureInfo t = m_font->PrintToTexture(*m_fontShader, d.text);
            m_lines.push_back({ t, d.size });
        }
    }

    Logger::Instance().Log(Logger::Severity::Info, "CreditsState Initialize");
}

// ─────────────────────────────────────────────────────────────────────────────
void CreditsState::Update(double /*dt*/)
{
    auto& input = gsm.GetEngine().GetInput();

    if (m_waitForRelease)
    {
        bool held = input.IsKeyPressed(Input::Key::Enter) ||
                    input.IsKeyPressed(Input::Key::Escape);
        if (!held) m_waitForRelease = false;
        return;
    }

    if (input.IsKeyTriggered(Input::Key::Escape) ||
        input.IsKeyTriggered(Input::Key::Enter))
    {
        Logger::Instance().Log(Logger::Severity::Event, "CreditsState: returning to MainMenu");
        gsm.PopState();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void CreditsState::DrawRect(const Math::Matrix& proj,
                             float cx, float cy, float w, float h,
                             float r, float g, float b, float a)
{
    Math::Matrix model = Math::Matrix::CreateTranslation({ cx, cy })
                       * Math::Matrix::CreateScale({ w, h });
    m_colorShader->setMat4("projection", proj);
    m_colorShader->setMat4("model", model);
    m_colorShader->setVec3("objectColor", r, g, b);
    m_colorShader->setFloat("uAlpha", a);
    GL::BindVertexArray(m_rectVAO);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);
    GL::BindVertexArray(0);
}

void CreditsState::Draw()
{
    GL::ClearColor(0.04f, 0.04f, 0.08f, 1.0f);
    GL::Clear(GL_COLOR_BUFFER_BIT);

    Math::Matrix proj = Math::Matrix::CreateOrtho(
        0.0f, CW, 0.0f, CH, -1.0f, 1.0f);

    GL::Enable(GL_BLEND);
    GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_colorShader->use();

    // Cyan title accent bar
    DrawRect(proj, CW * 0.5f, CH - 100.0f, CW, 4.0f, 0.0f, 1.0f, 1.0f, 0.6f);
    // Bottom accent bar
    DrawRect(proj, CW * 0.5f, 70.0f, CW, 4.0f, 1.0f, 0.0f, 1.0f, 0.6f);

    // ── Render text lines from top down ───────────────────────────────────
    if (m_font)
    {
        m_fontShader->use();
        m_fontShader->setMat4("projection", proj);

        constexpr float TOP_Y      = CH - 130.0f;
        constexpr float LINE_GAP   = 14.0f;
        constexpr float EMPTY_GAP  = 26.0f;

        float curY = TOP_Y;
        for (auto& ln : m_lines)
        {
            if (ln.size == 0.0f)
            {
                curY -= EMPTY_GAP;
                continue;
            }

            // Scale factor to reach desired display height
            float scale = (m_font->m_fontHeight > 0)
                ? ln.size / static_cast<float>(m_font->m_fontHeight)
                : 1.0f;
            float dispW = static_cast<float>(ln.tex.width) * scale;
            float dispH = ln.size;

            Math::Vec2 pos = {
                (CW - dispW) * 0.5f,
                curY - dispH
            };

            m_fontShader->setFloat("alpha", 1.0f);
            m_font->DrawBakedText(*m_fontShader, ln.tex, pos, ln.size);

            curY -= dispH + LINE_GAP;
        }
    }

    GL::Disable(GL_BLEND);
}

// ─────────────────────────────────────────────────────────────────────────────
void CreditsState::Shutdown()
{
#ifdef __EMSCRIPTEN__
    EM_ASM({
        window.gameCreditsActive = false;
    });
#endif

    if (m_font)       m_font->Shutdown();
    if (m_fontShader) m_fontShader.reset();
    if (m_colorShader) m_colorShader.reset();

    if (m_rectVAO != 0)
    {
        GL::DeleteVertexArrays(1, &m_rectVAO);
        GL::DeleteBuffers(1, &m_rectVBO);
        m_rectVAO = 0;
        m_rectVBO = 0;
    }

    Logger::Instance().Log(Logger::Severity::Info, "CreditsState Shutdown");
}

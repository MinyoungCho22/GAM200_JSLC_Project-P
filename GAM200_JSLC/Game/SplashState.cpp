// SplashState.cpp

#include "SplashState.hpp"
#include "MainMenu.hpp"
#include "../Engine/GameStateManager.hpp"
#include "../Engine/Engine.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/Logger.hpp"
#include "../OpenGL/GLWrapper.hpp"

#pragma warning(push, 0)
#include <stb_image.h>
#pragma warning(pop)

#include <cmath>
#include <algorithm>

SplashState::SplashState(GameStateManager& gsm_ref) : gsm(gsm_ref) {}

// ─────────────────────────────────────────────────────────────────────────────
void SplashState::Initialize()
{
    Logger::Instance().Log(Logger::Severity::Info, "SplashState Initialize");

    m_elapsed     = 0.0;
    m_alpha       = 0.0f;
    m_skip        = false;

    GL::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    GL::Clear(GL_COLOR_BUFFER_BIT);

    // ── Shaders ───────────────────────────────────────────────────────────
    m_splashShader = std::make_unique<Shader>(
        "OpenGL/Shaders/splash.vert",
        "OpenGL/Shaders/splash.frag");
    m_splashShader->use();
    m_splashShader->setInt("ourTexture", 0);

    m_silhouetteShader = std::make_unique<Shader>(
        "OpenGL/Shaders/simple.vert",
        "OpenGL/Shaders/silhouette.frag");
    m_silhouetteShader->use();
    m_silhouetteShader->setInt("ourTexture", 0);

    // ── Geometry – centred unit quad (-0.5..0.5) with UV ─────────────────
    float verts[] = {
        -0.5f,  0.5f,   0.0f, 1.0f,
         0.5f, -0.5f,   1.0f, 0.0f,
        -0.5f, -0.5f,   0.0f, 0.0f,
        -0.5f,  0.5f,   0.0f, 1.0f,
         0.5f,  0.5f,   1.0f, 1.0f,
         0.5f, -0.5f,   1.0f, 0.0f
    };
    GL::GenVertexArrays(1, &m_VAO);
    GL::GenBuffers(1, &m_VBO);
    GL::BindVertexArray(m_VAO);
    GL::BindBuffer(GL_ARRAY_BUFFER, m_VBO);
    GL::BufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    // attrib 0: position (2 floats)
    GL::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    GL::EnableVertexAttribArray(0);
    // attrib 1: UV (2 floats) — not used by solid_color.vert, ignored silently
    GL::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    GL::EnableVertexAttribArray(1);
    GL::BindVertexArray(0);

    // ── Texture loader helper ─────────────────────────────────────────────
    auto loadTex = [&](const char* path, unsigned int& id, int& w, int& h)
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
            GL::TexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, d);
            GL::GenerateMipmap(GL_TEXTURE_2D);
            stbi_image_free(d);
        }
        else
        {
            Logger::Instance().Log(Logger::Severity::Error,
                "SplashState: failed to load texture");
            w = h = 1;
            unsigned char white[] = { 255, 255, 255, 255 };
            GL::TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
        }
        GL::BindTexture(GL_TEXTURE_2D, 0);
    };

    loadTex("Asset/DigiPen.png",        m_texID,       m_texW,       m_texH);
    loadTex("Asset/Player_Walking.png", m_playerTexID, m_playerTexW, m_playerTexH);
    loadTex("Asset/Robot.png",          m_robotTexID,  m_robotTexW,  m_robotTexH);
    loadTex("Asset/Drone.png",          m_droneTexID,  m_droneTexW,  m_droneTexH);

    // ── Audio ─────────────────────────────────────────────────────────────
    m_logoSound.Load("Asset/DigiPen.mp3");
    m_logoSound.Play();
    m_logoSound.SetVolume(0.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
void SplashState::Update(double dt)
{
    SoundSystem::Instance().Update();

    // Skip on any key / mouse press
    Engine& engine = gsm.GetEngine();
    auto&   input  = engine.GetInput();
    if (!m_skip &&
        (input.IsKeyTriggered(Input::Key::Space) ||
         input.IsKeyTriggered(Input::Key::Enter) ||
         input.IsKeyTriggered(Input::Key::Escape)))
    {
        m_skip = true;
    }

    // Phase boundary helpers
    const double P_HOLD_START  = T_FADE_IN + T_WAVE1;
    const double P_BLOOM_START = P_HOLD_START + T_HOLD;

    // Skip: jump directly to bloom phase
    if (m_skip && m_elapsed < P_BLOOM_START)
        m_elapsed = P_BLOOM_START;

    m_elapsed += dt;

    // ── Alpha (logo opacity) ──────────────────────────────────────────────
    if (m_elapsed < T_FADE_IN)
    {
        float t  = static_cast<float>(m_elapsed / T_FADE_IN);
        m_alpha  = t * t;                         // ease-in
    }
    else if (m_elapsed < P_BLOOM_START)
    {
        m_alpha = 1.0f;
    }
    else
    {
        // Linear fade over T_BLOOM_OUT.
        float t  = static_cast<float>((m_elapsed - P_BLOOM_START) / T_BLOOM_OUT);
        t        = std::min(t, 1.0f);
        m_alpha  = 1.0f - t;
    }
    m_alpha = std::max(0.0f, std::min(1.0f, m_alpha));

    // ── BrightFloor + ShadowStr ───────────────────────────────────────────
    constexpr float BASE        = 2.0f;    // brightness during shadow-run
    constexpr float BRIGHT      = 4.0f;    // brightness during bright-hold
    constexpr float SUPER_BRIGHT = 9.0f;   // max brightness during bloom burst
    constexpr double RAMP       = 0.28;    // transition ramp duration (seconds)
    constexpr double BLOOM_RAMP = 0.40;    // how fast the bloom ramps up

    if (m_elapsed < P_HOLD_START)
    {
        // Shadow-run phase 1 – characters running, dark shadow sweeps logo
        m_brightFloor = BASE;
        m_shadowStr   = 1.0f;
    }
    else if (m_elapsed < P_HOLD_START + RAMP)
    {
        // Ramp from shadow → bright-hold
        float t       = static_cast<float>((m_elapsed - P_HOLD_START) / RAMP);
        float smooth  = t * t * (3.0f - 2.0f * t);          // smoothstep
        m_brightFloor = BASE + (BRIGHT - BASE) * smooth;
        m_shadowStr   = 1.0f - smooth;
    }
    else if (m_elapsed < P_BLOOM_START)
    {
        // Full bright hold – no shadow
        m_brightFloor = BRIGHT;
        m_shadowStr   = 0.0f;
    }
    else
    {
        // ── Bloom-fade-out phase ──────────────────────────────────────────
        m_shadowStr = 0.0f;

        // Bloom ramp: BRIGHT → SUPER_BRIGHT over the first BLOOM_RAMP seconds
        double bloomElapsed = m_elapsed - P_BLOOM_START;
        float  rampT        = static_cast<float>(
            std::min(bloomElapsed / BLOOM_RAMP, 1.0));
        float  rampSmooth   = rampT * rampT * (3.0f - 2.0f * rampT); // smoothstep
        m_brightFloor       = BRIGHT + (SUPER_BRIGHT - BRIGHT) * rampSmooth;
    }

    // Sync audio volume with visual alpha
    m_logoSound.SetVolume(m_alpha);

    // ── Transition ────────────────────────────────────────────────────────
    if (m_elapsed >= TOTAL)
        gsm.ChangeState(std::make_unique<MainMenu>(gsm));
}

// ─────────────────────────────────────────────────────────────────────────────
void SplashState::Draw()
{
    GL::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    GL::Clear(GL_COLOR_BUFFER_BIT);

    GL::Enable(GL_BLEND);
    GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Engine& engine  = gsm.GetEngine();
    float   screenW = static_cast<float>(engine.GetWidth());
    float   screenH = static_cast<float>(engine.GetHeight());

    Math::Matrix projection = Math::Matrix::CreateOrtho(
        0.0f, screenW, 0.0f, screenH, -1.0f, 1.0f);

    // ── DigiPen logo with bloom shader ────────────────────────────────────
    float logoW = screenW * 0.36f;
    float logoH = (m_texH > 0 && m_texW > 0)
                      ? logoW * (static_cast<float>(m_texH) / static_cast<float>(m_texW))
                      : logoW * 0.25f;

    Math::Vec2   center    = { screenW * 0.5f, screenH * 0.5f };
    Math::Matrix logoModel = Math::Matrix::CreateTranslation(center)
                           * Math::Matrix::CreateScale({ logoW, logoH });

    float texelW = (m_texW > 0) ? 1.0f / static_cast<float>(m_texW) : 0.001f;
    float texelH = (m_texH > 0) ? 1.0f / static_cast<float>(m_texH) : 0.001f;

    m_splashShader->use();
    m_splashShader->setMat4("projection",   projection);
    m_splashShader->setMat4("model",        logoModel);
    m_splashShader->setFloat("uTime",       static_cast<float>(m_elapsed));
    m_splashShader->setFloat("uAlpha",      m_alpha);
    m_splashShader->setFloat("uBrightFloor", m_brightFloor);
    m_splashShader->setFloat("uShadowStr",  m_shadowStr);
    m_splashShader->setVec2("uTexelSize",   texelW, texelH);

    GL::ActiveTexture(GL_TEXTURE0);
    GL::BindTexture(GL_TEXTURE_2D, m_texID);
    GL::BindVertexArray(m_VAO);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);
    GL::BindVertexArray(0);
    GL::BindTexture(GL_TEXTURE_2D, 0);

    // ── Character silhouettes (shadow-run phases only) ────────────────────
    if (m_playerTexID && m_robotTexID && m_droneTexID)
    {
        float t = static_cast<float>(m_elapsed);

        // ── Character sizes ───────────────────────────────────────────────
        constexpr float GIRL_H  = 110.0f;
        constexpr float ROBOT_H = 120.0f;
        constexpr float DRONE_H =  80.0f;

        float frameW  = (m_playerTexW > 0)
                        ? static_cast<float>(m_playerTexW) / PLAYER_FRAMES : 1.0f;
        float girlW   = (m_playerTexH > 0)
                        ? GIRL_H  * (frameW / static_cast<float>(m_playerTexH))
                        : GIRL_H  * 0.55f;
        float robotW  = (m_robotTexH > 0)
                        ? ROBOT_H * (static_cast<float>(m_robotTexW) / static_cast<float>(m_robotTexH))
                        : ROBOT_H * 0.65f;
        float droneW  = (m_droneTexH > 0)
                        ? DRONE_H * (static_cast<float>(m_droneTexW) / static_cast<float>(m_droneTexH))
                        : DRONE_H;

        // ── Running position (left→right, looping) ────────────────────────
        constexpr float SPEED    = 420.0f;   // pixels / second
        float cycleDist = screenW + girlW * 2.0f + 320.0f;
        // Single pass only: do not loop back to the left during splash.
        float progress  = std::min(t * SPEED, cycleDist);

        // Girl: leading the group
        float girlX  = progress - girlW;
        // Drone: 100px behind the girl (between girl and robot), flying above
        float droneX = girlX - 100.0f;
        // Robot: 220px behind the girl (chasing at the rear)
        float robotX = std::fmod(progress - 220.0f + cycleDist, cycleDist) - girlW;

        // Keep silhouettes fully visible until all characters have fully passed the screen.
        bool allPassed =
            (girlX  - girlW  * 0.5f > screenW) &&
            (droneX - droneW * 0.5f > screenW) &&
            (robotX - robotW * 0.5f > screenW);
        float charAlpha = allPassed ? (m_alpha * m_shadowStr) : m_alpha;
        if (charAlpha <= 0.01f)
        {
            GL::Disable(GL_BLEND);
            return;
        }

        // ── Y positions with running / hover bobs ────────────────────────
        float midY   = screenH * 0.5f;
        float girlY  = midY + std::sinf(t * 18.0f) * 5.0f;
        float robotY = midY + std::sinf(t * 13.0f + 0.8f) * 3.5f;
        float droneY = midY + 75.0f + std::sinf(t * 7.0f + 1.5f) * 9.0f; // hovering above

        // ── Player walk-cycle frame (7 frames @ ~10 fps) ─────────────────
        int   frameIdx = static_cast<int>(t * 10.0f) % PLAYER_FRAMES;
        float fU       = 1.0f / static_cast<float>(PLAYER_FRAMES);

        m_silhouetteShader->use();
        m_silhouetteShader->setMat4("projection", projection);
        m_silhouetteShader->setFloat("alpha", charAlpha);

        GL::ActiveTexture(GL_TEXTURE0);
        GL::BindVertexArray(m_VAO);

        // ── Draw robot (furthest back; flipX = true to face right) ────────
        m_silhouetteShader->setBool("flipX",   true);
        m_silhouetteShader->setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
        {
            Math::Matrix mdl = Math::Matrix::CreateTranslation({ robotX, robotY })
                             * Math::Matrix::CreateScale({ robotW, ROBOT_H });
            m_silhouetteShader->setMat4("model", mdl);
        }
        GL::BindTexture(GL_TEXTURE_2D, m_robotTexID);
        GL::DrawArrays(GL_TRIANGLES, 0, 6);

        // ── Draw drone (between robot and girl, hovering above) ───────────
        m_silhouetteShader->setBool("flipX",   false);
        m_silhouetteShader->setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
        {
            Math::Matrix mdl = Math::Matrix::CreateTranslation({ droneX, droneY })
                             * Math::Matrix::CreateScale({ droneW, DRONE_H });
            m_silhouetteShader->setMat4("model", mdl);
        }
        GL::BindTexture(GL_TEXTURE_2D, m_droneTexID);
        GL::DrawArrays(GL_TRIANGLES, 0, 6);

        // ── Draw girl (front; Player_Walking.png sprite sheet) ────────────
        m_silhouetteShader->setBool("flipX",   false);
        m_silhouetteShader->setVec4("spriteRect", frameIdx * fU, 0.0f, fU, 1.0f);
        {
            Math::Matrix mdl = Math::Matrix::CreateTranslation({ girlX, girlY })
                             * Math::Matrix::CreateScale({ girlW, GIRL_H });
            m_silhouetteShader->setMat4("model", mdl);
        }
        GL::BindTexture(GL_TEXTURE_2D, m_playerTexID);
        GL::DrawArrays(GL_TRIANGLES, 0, 6);

        GL::BindVertexArray(0);
        GL::BindTexture(GL_TEXTURE_2D, 0);
    }

    GL::Disable(GL_BLEND);
}

// ─────────────────────────────────────────────────────────────────────────────
void SplashState::Shutdown()
{
    m_logoSound.Stop();

    if (m_VAO)         { GL::DeleteVertexArrays(1, &m_VAO);        m_VAO         = 0; }
    if (m_VBO)         { GL::DeleteBuffers(1, &m_VBO);             m_VBO         = 0; }
    if (m_texID)       { GL::DeleteTextures(1, &m_texID);          m_texID       = 0; }
    if (m_playerTexID) { GL::DeleteTextures(1, &m_playerTexID);    m_playerTexID = 0; }
    if (m_robotTexID)  { GL::DeleteTextures(1, &m_robotTexID);     m_robotTexID  = 0; }
    if (m_droneTexID)  { GL::DeleteTextures(1, &m_droneTexID);     m_droneTexID  = 0; }

    m_splashShader.reset();
    m_silhouetteShader.reset();

    Logger::Instance().Log(Logger::Severity::Info, "SplashState Shutdown");
}

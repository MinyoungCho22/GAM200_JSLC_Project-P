// Setting.cpp

#include "Setting.hpp"
#include "StoryDialogue.hpp"
#include "../Engine/GameStateManager.hpp"
#include "../Engine/Engine.hpp"
#include "../Engine/Sound.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/Logger.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include <string>
#include <cmath>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// Fixed logical resolution for the UI system
constexpr float GAME_WIDTH  = static_cast<float>(VIRTUAL_WIDTH);
constexpr float GAME_HEIGHT = static_cast<float>(VIRTUAL_HEIGHT);

// ---- Layout constants -------------------------------------------------------
constexpr float COL_LABEL  = 560.0f;   // x: label column (right-edge aligned)
constexpr float COL_VALUE  = 980.0f;   // x: value column (left-edge)
constexpr float ROW_TITLE      = 870.0f;
constexpr float ROW_FPS        = 730.0f;
constexpr float ROW_VSYNC      = 630.0f;
constexpr float ROW_FULLSCREEN = 530.0f;
constexpr float ROW_VOLUME     = 430.0f;
constexpr float ROW_PAD_AIM    = 330.0f;
constexpr float ROW_DIALOGUE   = 230.0f;
constexpr float ROW_EXIT       = 130.0f;
// Hints at bottom (ortho y-up): keep clear gap below Exit row + highlight bar.
constexpr float ROW_HINT_WASD = 95.0f;
constexpr float ROW_HINT_ESC  = 42.0f;

constexpr float LABEL_SIZE = 52.0f;
constexpr float VALUE_SIZE = 52.0f;
constexpr float TITLE_SIZE = 76.0f;
constexpr float HINT_SIZE  = 34.0f;

constexpr float BAR_W = 480.0f;
constexpr float BAR_H =  28.0f;
constexpr float BAR_CX = COL_VALUE + BAR_W * 0.5f;  // bar center x

// ---- FPS option table -------------------------------------------------------
constexpr int SettingState::FPS_VALUES[SettingState::FPS_COUNT];

// -----------------------------------------------------------------------------

SettingState::SettingState(GameStateManager& gsm_ref, bool exitToMainMenu)
    : gsm(gsm_ref), m_exitToMainMenu(exitToMainMenu)
{
}

const char* SettingState::GetFpsLabel(int index) const
{
    switch (index)
    {
    case 0: return "30 FPS";
    case 1: return "60 FPS";
    case 2: return "144 FPS";
    case 3: return "240 FPS";
    case 4: return "No Limit";
    default: return "?";
    }
}

// -----------------------------------------------------------------------------
// Apply helpers
// -----------------------------------------------------------------------------
void SettingState::ApplyFps()
{
    gsm.GetEngine().SetFpsCap(FPS_VALUES[m_fpsIndex]);
}

void SettingState::ApplyVSync()
{
    gsm.GetEngine().SetVSync(m_vsyncEnabled);
}

void SettingState::ApplyFullscreen()
{
    gsm.GetEngine().SetFullscreen(m_fullscreenEnabled);
}

void SettingState::ApplyVolume()
{
    SoundSystem::Instance().SetMasterVolume(m_masterVolume);
}

void SettingState::ApplyPadAim()
{
    m_padAimSensitivity = std::max(PAD_AIM_MIN, std::min(PAD_AIM_MAX, m_padAimSensitivity));
    gsm.GetEngine().GetInput().SetGamepadAimSensitivity(m_padAimSensitivity);
}

void SettingState::ApplyDialogue()
{
    StoryDialogue::SetDialogueEnabled(m_dialogueEnabled);
}

// -----------------------------------------------------------------------------
// Dynamic text rebuild
// -----------------------------------------------------------------------------
void SettingState::RebuildValueTexts()
{
    // FPS value string
    {
        std::string s = std::string("< ") + GetFpsLabel(m_fpsIndex) + " >";
        m_fpsValueText = m_font->PrintToTexture(*m_fontShader, s);
    }

    // VSync value string
    {
        std::string s = std::string("< ") + (m_vsyncEnabled ? "On" : "Off") + " >";
        m_vsyncValueText = m_font->PrintToTexture(*m_fontShader, s);
    }

    // Fullscreen value string
    {
        std::string s = std::string("< ") + (m_fullscreenEnabled ? "On" : "Off") + " >";
        m_fullscreenValueText = m_font->PrintToTexture(*m_fontShader, s);
    }

    // Volume percentage string
    {
        int pct = static_cast<int>(std::round(m_masterVolume * 100.0f));
        m_volumePctText = m_font->PrintToTexture(*m_fontShader, std::to_string(pct) + "%");
    }

    // Gamepad right-stick aim sensitivity (50%–200%)
    {
        int pct = static_cast<int>(std::round(m_padAimSensitivity * 100.0f));
        std::string s = std::string("< ") + std::to_string(pct) + "% >";
        m_padAimValueText = m_font->PrintToTexture(*m_fontShader, s);
    }

    // Dialogue text on/off
    {
        std::string s = std::string("< ") + (m_dialogueEnabled ? "On" : "Off") + " >";
        m_dialogueValueText = m_font->PrintToTexture(*m_fontShader, s);
    }
}

// -----------------------------------------------------------------------------
// Initialize
// -----------------------------------------------------------------------------
void SettingState::Initialize()
{
#ifdef __EMSCRIPTEN__
    EM_ASM({
        window.gameSettingsActive = true;
    });
#endif

    Logger::Instance().Log(Logger::Severity::Info, "SettingState Initialize");

    m_font = std::make_unique<Font>();
    m_font->Initialize("Asset/fonts/Font_Outlined.png");

    m_colorShader = std::make_unique<Shader>("OpenGL/Shaders/solid_color.vert",
                                             "OpenGL/Shaders/solid_color.frag");
    m_fontShader  = std::make_unique<Shader>("OpenGL/Shaders/simple.vert",
                                             "OpenGL/Shaders/simple.frag");
    m_fontShader->use();
    m_fontShader->setInt("ourTexture", 0);

    // Sync state from Engine / SoundSystem
    Engine& engine = gsm.GetEngine();
    m_vsyncEnabled = engine.IsVSyncEnabled();
    m_fullscreenEnabled = engine.IsFullscreen();
    m_masterVolume = SoundSystem::Instance().GetMasterVolume();
    m_padAimSensitivity = engine.GetInput().GetGamepadAimSensitivity();
    m_padAimSensitivity = std::max(PAD_AIM_MIN, std::min(PAD_AIM_MAX, m_padAimSensitivity));

    int currentCap = engine.GetFpsCap();
    m_fpsIndex = FPS_COUNT - 1; // default: No Limit
    for (int i = 0; i < FPS_COUNT; ++i)
    {
        if (FPS_VALUES[i] == currentCap) { m_fpsIndex = i; break; }
    }

    // Static text
    m_titleText       = m_font->PrintToTexture(*m_fontShader, "SETTINGS");
    m_fpsLabelText    = m_font->PrintToTexture(*m_fontShader, "FPS");
    m_vsyncLabelText  = m_font->PrintToTexture(*m_fontShader, "VSync");
    m_fullscreenLabelText = m_font->PrintToTexture(*m_fontShader, "Fullscreen");
    m_volumeLabelText = m_font->PrintToTexture(*m_fontShader, "Volume");
    m_padAimLabelText    = m_font->PrintToTexture(*m_fontShader, "Pad Aim");
    m_dialogueLabelText  = m_font->PrintToTexture(*m_fontShader, "Dialogue Text");
    m_exitText           = m_font->PrintToTexture(*m_fontShader, "Exit");
    m_wasdHintText    = m_font->PrintToTexture(*m_fontShader, "W: Up   S: Down   A: Left   D: Right");
    m_escHintText     = m_font->PrintToTexture(*m_fontShader, "[ESC] Back");

    m_dialogueEnabled = StoryDialogue::IsDialogueEnabled();

    ApplyPadAim();
    RebuildValueTexts();

    // ---- Fullscreen overlay quad (0..1 range) --------------------------------
    float overlayVerts[] = {
        0.0f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f,
        0.0f, 1.0f,  1.0f, 1.0f,  1.0f, 0.0f
    };
    GL::GenVertexArrays(1, &m_overlayVAO);
    GL::GenBuffers(1, &m_overlayVBO);
    GL::BindVertexArray(m_overlayVAO);
    GL::BindBuffer(GL_ARRAY_BUFFER, m_overlayVBO);
    GL::BufferData(GL_ARRAY_BUFFER, sizeof(overlayVerts), overlayVerts, GL_STATIC_DRAW);
    GL::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    GL::EnableVertexAttribArray(0);
    GL::BindVertexArray(0);

    // ---- Centered unit quad (-0.5..0.5) for bar drawing ---------------------
    float barVerts[] = {
        -0.5f,  0.5f,
         0.5f,  0.5f,
        -0.5f, -0.5f,
         0.5f,  0.5f,
         0.5f, -0.5f,
        -0.5f, -0.5f
    };
    GL::GenVertexArrays(1, &m_barVAO);
    GL::GenBuffers(1, &m_barVBO);
    GL::BindVertexArray(m_barVAO);
    GL::BindBuffer(GL_ARRAY_BUFFER, m_barVBO);
    GL::BufferData(GL_ARRAY_BUFFER, sizeof(barVerts), barVerts, GL_STATIC_DRAW);
    GL::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    GL::EnableVertexAttribArray(0);
    GL::BindVertexArray(0);

    m_colorShader->use();
    m_colorShader->setFloat("uAlpha", 1.0f);
}

// -----------------------------------------------------------------------------
// Update
// -----------------------------------------------------------------------------
void SettingState::Update(double /*dt*/)
{
    Engine& engine = gsm.GetEngine();
    auto& input = engine.GetInput();

    if (input.IsKeyTriggered(Input::Key::Escape))
    {
        gsm.PopState();
        return;
    }

    // --- Navigate up / down ---
    bool moveUp   = input.IsKeyTriggered(Input::Key::W);
    bool moveDown = input.IsKeyTriggered(Input::Key::S);

    if (moveUp)
    {
        int idx = static_cast<int>(m_selectedItem);
        idx = (idx + MENU_ITEM_COUNT - 1) % MENU_ITEM_COUNT;
        m_selectedItem = static_cast<MenuItem>(idx);
    }
    else if (moveDown)
    {
        int idx = static_cast<int>(m_selectedItem);
        idx = (idx + 1) % MENU_ITEM_COUNT;
        m_selectedItem = static_cast<MenuItem>(idx);
    }

    // --- Change value left / right ---
    bool goLeft  = input.IsKeyTriggered(Input::Key::A);
    bool goRight = input.IsKeyTriggered(Input::Key::D);

    if (m_selectedItem == MenuItem::FPS && (goLeft || goRight))
    {
        m_fpsIndex = (m_fpsIndex + (goRight ? 1 : FPS_COUNT - 1)) % FPS_COUNT;
        ApplyFps();
        RebuildValueTexts();
    }
    else if (m_selectedItem == MenuItem::VSync && (goLeft || goRight))
    {
        m_vsyncEnabled = !m_vsyncEnabled;
        ApplyVSync();
        RebuildValueTexts();
    }
    else if (m_selectedItem == MenuItem::Fullscreen && (goLeft || goRight))
    {
        m_fullscreenEnabled = !m_fullscreenEnabled;
        ApplyFullscreen();
        RebuildValueTexts();
    }
    else if (m_selectedItem == MenuItem::Volume)
    {
        bool changed = false;
        if (goRight) { m_masterVolume = std::min(1.0f, m_masterVolume + VOLUME_STEP); changed = true; }
        if (goLeft)  { m_masterVolume = std::max(0.0f, m_masterVolume - VOLUME_STEP); changed = true; }
        if (changed)
        {
            ApplyVolume();
            RebuildValueTexts();
        }
    }
    else if (m_selectedItem == MenuItem::PadAim && (goLeft || goRight))
    {
        if (goRight)
            m_padAimSensitivity = std::min(PAD_AIM_MAX, m_padAimSensitivity + PAD_AIM_STEP);
        else
            m_padAimSensitivity = std::max(PAD_AIM_MIN, m_padAimSensitivity - PAD_AIM_STEP);
        ApplyPadAim();
        RebuildValueTexts();
    }
    else if (m_selectedItem == MenuItem::Dialogue && (goLeft || goRight))
    {
        m_dialogueEnabled = !m_dialogueEnabled;
        ApplyDialogue();
        RebuildValueTexts();
    }
    else if (m_selectedItem == MenuItem::Exit)
    {
        if (input.IsKeyTriggered(Input::Key::Enter) || input.IsKeyTriggered(Input::Key::Space))
        {
            if (m_exitToMainMenu)
            {
                // Defer the stack clear to Engine::Step() to avoid destroying 'this' mid-Update
                engine.RequestReturnToMainMenu();
                return;
            }
#ifdef __EMSCRIPTEN__
            engine.RequestReturnToSplash();
            return;
#else
            engine.RequestShutdown();
#endif
        }
    }
}

// -----------------------------------------------------------------------------
// DrawRect helper (centered at cx, cy)
// -----------------------------------------------------------------------------
void SettingState::DrawRect(const Math::Matrix& proj, float cx, float cy,
                            float w, float h,
                            float r, float g, float b, float a)
{
    Math::Matrix model = Math::Matrix::CreateTranslation({ cx, cy }) *
                         Math::Matrix::CreateScale({ w, h });
    m_colorShader->setMat4("model", model);
    m_colorShader->setMat4("projection", proj);
    m_colorShader->setVec3("objectColor", r, g, b);
    m_colorShader->setFloat("uAlpha", a);
    GL::BindVertexArray(m_barVAO);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);
    GL::BindVertexArray(0);
}

// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------
void SettingState::Draw()
{
    // Viewport is already set to FBO size (GAME_WIDTH x GAME_HEIGHT) by PostProcessManager::BeginScene().
    // Do NOT call glfwGetFramebufferSize here — on Retina displays the physical framebuffer is larger
    // than the FBO, which would cause only the bottom-left portion to be rendered.

    Math::Matrix proj = Math::Matrix::CreateOrtho(0.0f, GAME_WIDTH, 0.0f, GAME_HEIGHT, -1.0f, 1.0f);

    GL::Enable(GL_BLEND);
    GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // 1. Opaque black background
    m_colorShader->use();
    m_colorShader->setMat4("projection", proj);
    {
        Math::Matrix model = Math::Matrix::CreateTranslation({ GAME_WIDTH / 2.0f, GAME_HEIGHT / 2.0f }) *
                             Math::Matrix::CreateScale({ GAME_WIDTH, GAME_HEIGHT });
        m_colorShader->setMat4("model", model);
        m_colorShader->setVec3("objectColor", 0.0f, 0.0f, 0.0f);
        m_colorShader->setFloat("uAlpha", 1.0f);
        GL::BindTexture(GL_TEXTURE_2D, 0);
        GL::BindVertexArray(m_overlayVAO);
        GL::DrawArrays(GL_TRIANGLES, 0, 6);
        GL::BindVertexArray(0);
    }

    // 2. Separator line below title
    DrawRect(proj, GAME_WIDTH * 0.5f, ROW_TITLE - 60.0f, 900.0f, 3.0f, 0.3f, 0.3f, 0.3f, 1.0f);

    // 3. Highlight bar behind selected item
    float selRowY = 0.0f;
    float selRowLeft = 0.0f;
    float selRowRight = 0.0f;
    switch (m_selectedItem)
    {
    case MenuItem::FPS:
        selRowY = ROW_FPS;
        selRowLeft = COL_LABEL - static_cast<float>(m_fpsLabelText.width) * (LABEL_SIZE / m_fpsLabelText.height);
        selRowRight = COL_VALUE + static_cast<float>(m_fpsValueText.width) * (VALUE_SIZE / m_fpsValueText.height);
        break;
    case MenuItem::VSync:
        selRowY = ROW_VSYNC;
        selRowLeft = COL_LABEL - static_cast<float>(m_vsyncLabelText.width) * (LABEL_SIZE / m_vsyncLabelText.height);
        selRowRight = COL_VALUE + static_cast<float>(m_vsyncValueText.width) * (VALUE_SIZE / m_vsyncValueText.height);
        break;
    case MenuItem::Fullscreen:
        selRowY = ROW_FULLSCREEN;
        selRowLeft = COL_LABEL - static_cast<float>(m_fullscreenLabelText.width) * (LABEL_SIZE / m_fullscreenLabelText.height);
        selRowRight = COL_VALUE + static_cast<float>(m_fullscreenValueText.width) * (VALUE_SIZE / m_fullscreenValueText.height);
        break;
    case MenuItem::Volume:
        selRowY = ROW_VOLUME;
        selRowLeft = COL_LABEL - static_cast<float>(m_volumeLabelText.width) * (LABEL_SIZE / m_volumeLabelText.height);
        selRowRight = BAR_CX + BAR_W * 0.5f + 24.0f + static_cast<float>(m_volumePctText.width) * (VALUE_SIZE / m_volumePctText.height);
        break;
    case MenuItem::PadAim:
        selRowY = ROW_PAD_AIM;
        selRowLeft = COL_LABEL - static_cast<float>(m_padAimLabelText.width) * (LABEL_SIZE / m_padAimLabelText.height);
        selRowRight = COL_VALUE + static_cast<float>(m_padAimValueText.width) * (VALUE_SIZE / m_padAimValueText.height);
        break;
    case MenuItem::Dialogue:
        selRowY = ROW_DIALOGUE;
        selRowLeft = COL_LABEL - static_cast<float>(m_dialogueLabelText.width) * (LABEL_SIZE / m_dialogueLabelText.height);
        selRowRight = COL_VALUE + static_cast<float>(m_dialogueValueText.width) * (VALUE_SIZE / m_dialogueValueText.height);
        break;
    case MenuItem::Exit:
        selRowY = ROW_EXIT;
        selRowLeft = GAME_WIDTH * 0.5f - static_cast<float>(m_exitText.width) * (VALUE_SIZE / m_exitText.height) * 0.5f;
        selRowRight = GAME_WIDTH * 0.5f + static_cast<float>(m_exitText.width) * (VALUE_SIZE / m_exitText.height) * 0.5f;
        break;
    }
    const float barPaddingX = 28.0f;
    float highlightW = (selRowRight - selRowLeft) + barPaddingX * 2.0f;
    float highlightCX = (selRowLeft + selRowRight) * 0.5f;
    DrawRect(proj, highlightCX, selRowY + LABEL_SIZE * 0.3f,
             highlightW, LABEL_SIZE + 12.0f, 0.05f, 0.25f, 0.25f, 0.85f);

    // 4. Volume bar (background + fill)
    {                              
        // Background
        DrawRect(proj, BAR_CX, ROW_VOLUME + LABEL_SIZE * 0.3f,
                 BAR_W, BAR_H, 0.2f, 0.2f, 0.2f, 1.0f);

        // Fill (neon cyan tint)
        float fillW = BAR_W * m_masterVolume;
        if (fillW > 1.0f)
        {
            float fillCX = (BAR_CX - BAR_W * 0.5f) + fillW * 0.5f;
            DrawRect(proj, fillCX, ROW_VOLUME + LABEL_SIZE * 0.3f,
                     fillW, BAR_H, 0.0f, 0.85f, 0.75f, 1.0f);
        }
    }

    // 5. Text rendering
    m_fontShader->use();
    m_fontShader->setMat4("projection", proj);

    auto drawLabel = [&](const CachedTextureInfo& tex, float y)
    {
        m_font->DrawBakedText(*m_fontShader, tex,
                              { COL_LABEL - static_cast<float>(tex.width) * (LABEL_SIZE / tex.height), y },
                              LABEL_SIZE);
    };
    auto drawValue = [&](const CachedTextureInfo& tex, float y)
    {
        m_font->DrawBakedText(*m_fontShader, tex, { COL_VALUE, y }, VALUE_SIZE);
    };

    // Title
    m_font->DrawBakedText(*m_fontShader, m_titleText,
                          { GAME_WIDTH * 0.5f - m_titleText.width * (TITLE_SIZE / m_titleText.height) * 0.5f,
                            ROW_TITLE },
                          TITLE_SIZE);

    // FPS row
    drawLabel(m_fpsLabelText, ROW_FPS);
    drawValue(m_fpsValueText, ROW_FPS);

    // VSync row
    drawLabel(m_vsyncLabelText, ROW_VSYNC);
    drawValue(m_vsyncValueText, ROW_VSYNC);

    // Fullscreen row
    drawLabel(m_fullscreenLabelText, ROW_FULLSCREEN);
    drawValue(m_fullscreenValueText, ROW_FULLSCREEN);

    // Volume row  (label + percentage; bar drawn above in color shader pass)
    drawLabel(m_volumeLabelText, ROW_VOLUME);
    {
        float pctX = BAR_CX + BAR_W * 0.5f + 24.0f;
        m_font->DrawBakedText(*m_fontShader, m_volumePctText, { pctX, ROW_VOLUME }, VALUE_SIZE);
    }

    // Gamepad aim sensitivity
    drawLabel(m_padAimLabelText, ROW_PAD_AIM);
    drawValue(m_padAimValueText, ROW_PAD_AIM);

    // Dialogue text on/off
    drawLabel(m_dialogueLabelText, ROW_DIALOGUE);
    drawValue(m_dialogueValueText, ROW_DIALOGUE);

    // Exit row
    if (m_selectedItem == MenuItem::Exit)
    {
        CachedTextureInfo exitSel = m_font->PrintToTexture(*m_fontShader, "> Exit <");
        m_font->DrawBakedText(*m_fontShader, exitSel,
                              { GAME_WIDTH * 0.5f - exitSel.width * (VALUE_SIZE / exitSel.height) * 0.5f,
                                ROW_EXIT },
                              VALUE_SIZE);
    }
    else
    {
        m_font->DrawBakedText(*m_fontShader, m_exitText,
                              { GAME_WIDTH * 0.5f - m_exitText.width * (VALUE_SIZE / m_exitText.height) * 0.5f,
                                ROW_EXIT },
                              VALUE_SIZE);
    }

    m_font->DrawBakedText(*m_fontShader, m_wasdHintText,
                          { GAME_WIDTH * 0.5f - m_wasdHintText.width * (HINT_SIZE / m_wasdHintText.height) * 0.5f,
                            ROW_HINT_WASD },
                          HINT_SIZE);

    m_font->DrawBakedText(*m_fontShader, m_escHintText,
                          { GAME_WIDTH * 0.5f - m_escHintText.width * (HINT_SIZE / m_escHintText.height) * 0.5f,
                            ROW_HINT_ESC },
                          HINT_SIZE);
}

void SettingState::Shutdown()
{
#ifdef __EMSCRIPTEN__
    EM_ASM({
        window.gameSettingsActive = false;
    });
#endif

    GL::DeleteVertexArrays(1, &m_overlayVAO);
    GL::DeleteBuffers(1, &m_overlayVBO);
    GL::DeleteVertexArrays(1, &m_barVAO);
    GL::DeleteBuffers(1, &m_barVBO);
    Logger::Instance().Log(Logger::Severity::Info, "SettingState Shutdown");
}

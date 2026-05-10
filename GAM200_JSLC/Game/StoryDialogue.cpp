//StoryDialogue.cpp

#include "StoryDialogue.hpp"
#include "../Engine/ControlBindings.hpp"
#include "Background.hpp"
#include "../Engine/Input.hpp"
#include "../Engine/Matrix.hpp"
#include "../OpenGL/Shader.hpp"
#include "../OpenGL/GLWrapper.hpp"

bool StoryDialogue::s_dialogueEnabled = true;

namespace
{
constexpr float GAME_WIDTH = 1920.0f;
constexpr float GAME_HEIGHT = 1080.0f;
constexpr float BOX_WIDTH = 1680.0f;
constexpr float BOX_HEIGHT = 240.0f;
constexpr float BOX_BOTTOM_MARGIN = 36.0f;
constexpr float TEXT_HEIGHT = 40.0f;
} // namespace

StoryDialogue::StoryDialogue() = default;

StoryDialogue::~StoryDialogue()
{
    Shutdown();
}

void StoryDialogue::Initialize()
{
    Shutdown();
    m_boxImage = std::make_unique<Background>();
    m_boxImage->Initialize("Asset/Conversion.png");
}

void StoryDialogue::Shutdown()
{
    ResetForNewRun();
    if (m_boxImage)
    {
        m_boxImage->Shutdown();
        m_boxImage.reset();
    }
}

void StoryDialogue::ResetForNewRun()
{
    ClearLineTexture();
    m_lines.clear();
    m_pending.clear();
    m_onSequenceComplete = nullptr;
    m_preTypeDelayRemaining = 0.0f;
    m_active                  = false;
    m_useConversionBackdrop   = true;
    m_blocksGameplay          = true;
    m_lineIndex = 0;
    m_visibleChars = 0;
    m_charAccum = 0.0f;
}

void StoryDialogue::ClearLineTexture()
{
    if (m_lineTex.textureID != 0)
    {
        GL::DeleteTextures(1, &m_lineTex.textureID);
        m_lineTex = {};
    }
}

void StoryDialogue::RebuildLineTexture(Font& font, Shader& fontShader)
{
    ClearLineTexture();
    if (!m_active || m_lineIndex >= m_lines.size())
        return;
    const std::string& full = m_lines[m_lineIndex];
    if (m_visibleChars == 0)
        return;
    const std::string sub = full.substr(0, m_visibleChars);
    m_lineTex = font.CreateTextTexture(fontShader, sub);
}

void StoryDialogue::BeginSequence(std::vector<std::string> lines, std::function<void()> onComplete, float preTypeDelay,
                                  bool useConversionBackdrop, bool blocksGameplay)
{
    ClearLineTexture();
    m_lines                   = std::move(lines);
    m_lineIndex               = 0;
    m_visibleChars            = 0;
    m_charAccum               = 0.0f;
    m_preTypeDelayRemaining   = preTypeDelay;
    m_onSequenceComplete      = std::move(onComplete);
    m_useConversionBackdrop   = useConversionBackdrop;
    m_blocksGameplay          = blocksGameplay;
    m_active                  = true;
}

void StoryDialogue::FinishSequence(Font& font, Shader& fontShader)
{
    auto done = std::move(m_onSequenceComplete);
    m_onSequenceComplete = nullptr;

    ClearLineTexture();
    m_lines.clear();
    m_lineIndex = 0;
    m_visibleChars = 0;
    m_preTypeDelayRemaining = 0.0f;
    m_active                  = false;
    m_useConversionBackdrop   = true;
    m_blocksGameplay          = true;

    if (done)
        done();

    if (!m_pending.empty())
    {
        QueuedSequence next = std::move(m_pending.front());
        m_pending.pop_front();
        BeginSequence(std::move(next.lines), std::move(next.onComplete), next.preTypeDelay,
                      next.useConversionBackdrop, next.blocksGameplay);
        RebuildLineTexture(font, fontShader);
    }
}

void StoryDialogue::EnqueueLines(const std::vector<std::string>& lines, Font& font, Shader& fontShader,
    std::function<void()> onSequenceComplete, bool useConversionBackdrop, bool blocksGameplay)
{
    if (lines.empty() || !s_dialogueEnabled)
        return;
    if (m_active)
    {
        m_pending.push_back({ lines, std::move(onSequenceComplete), 0.0f, useConversionBackdrop, blocksGameplay });
        return;
    }
    BeginSequence(lines, std::move(onSequenceComplete), 0.0f, useConversionBackdrop, blocksGameplay);
    RebuildLineTexture(font, fontShader);
}

void StoryDialogue::EnqueueOpening(Font& font, Shader& fontShader)
{
    QueuedSequence q;
    q.lines = {
        "...Where am I?",
        "I can't see anything. I should open the blinds and look outside first.",
    };
    q.preTypeDelay = 0.0f;
    q.onComplete = nullptr;

    if (m_active)
    {
        m_pending.push_back(std::move(q));
        return;
    }
    BeginSequence(std::move(q.lines), std::move(q.onComplete), q.preTypeDelay, q.useConversionBackdrop, q.blocksGameplay);
    RebuildLineTexture(font, fontShader);
}

void StoryDialogue::Update(float dt, const Input::Input& input, const ControlBindings& controls, Font& font, Shader& fontShader)
{
    if (!s_dialogueEnabled)
    {
        if (m_active)
            ResetForNewRun();
        return;
    }
    if (!m_active || m_lines.empty())
        return;

    if (m_preTypeDelayRemaining > 0.0f)
    {
        if (controls.IsActionTriggered(ControlAction::Attack, input))
            m_preTypeDelayRemaining = 0.0f;
        else
            m_preTypeDelayRemaining -= dt;
        if (m_preTypeDelayRemaining > 0.0f)
            return;
    }

    const std::string& line = m_lines[m_lineIndex];

    if (controls.IsActionTriggered(ControlAction::Attack, input))
    {
        if (m_visibleChars < line.size())
        {
            m_visibleChars = line.size();
            RebuildLineTexture(font, fontShader);
        }
        else
        {
            ++m_lineIndex;
            if (m_lineIndex >= m_lines.size())
            {
                FinishSequence(font, fontShader);
                if (m_active)
                    RebuildLineTexture(font, fontShader);
            }
            else
            {
                m_visibleChars = 0;
                m_charAccum = 0.0f;
                ClearLineTexture();
            }
        }
        return;
    }

    if (m_visibleChars < line.size())                       
    {
        m_charAccum += dt;
        while (m_charAccum >= m_secondsPerChar && m_visibleChars < line.size())
        {
            m_charAccum -= m_secondsPerChar;
            ++m_visibleChars;
            RebuildLineTexture(font, fontShader);
        }
    }
}

void StoryDialogue::Draw(Font& font, Shader& textureShader, Shader& fontShader, const Math::Matrix& screenProjection)
{
    if (!s_dialogueEnabled || !m_active)
        return;

    const float boxCenterX = GAME_WIDTH * 0.5f;
    const float boxCenterY = BOX_BOTTOM_MARGIN + BOX_HEIGHT * 0.5f;

    if (m_useConversionBackdrop && m_boxImage && m_boxImage->GetWidth() > 0)
    {
        GL::Enable(GL_BLEND);
        GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        textureShader.use();
        textureShader.setMat4("projection", screenProjection);
        textureShader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
        textureShader.setBool("flipX", false);
        textureShader.setFloat("alpha", 1.0f);
        textureShader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
        textureShader.setFloat("tintStrength", 0.0f);

        Math::Matrix boxModel = Math::Matrix::CreateTranslation({ boxCenterX, boxCenterY })
            * Math::Matrix::CreateScale({ BOX_WIDTH, BOX_HEIGHT });
        textureShader.setMat4("model", boxModel);
        m_boxImage->Draw(textureShader, boxModel);
    }

    if (m_lineTex.textureID != 0 && m_lineTex.height > 0)
    {
        const float rw = static_cast<float>(m_lineTex.width) * (TEXT_HEIGHT / static_cast<float>(font.m_fontHeight));
        const float posX = boxCenterX - rw * 0.5f;
        const float posY = boxCenterY - TEXT_HEIGHT * 0.35f;
        GL::Enable(GL_BLEND);
        GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        fontShader.use();
        fontShader.setMat4("projection", screenProjection);
        font.DrawBakedText(fontShader, m_lineTex, { posX, posY }, TEXT_HEIGHT);
    }

    GL::Disable(GL_BLEND);
}

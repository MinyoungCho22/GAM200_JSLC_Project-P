#pragma once

#include "../Engine/Matrix.hpp"
#include "../Engine/Vec2.hpp"
#include "Font.hpp"
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Input {
class Input;
}
class Shader;
class Background;

/// Full-screen narrative box (Conversion.png) with typewriter text; blocks gameplay while active.
class StoryDialogue
{
public:
    StoryDialogue();
    ~StoryDialogue();

    void Initialize();
    void Shutdown();
    /// Drop active line, queue, and UI state (e.g. new run after game over).
    void ResetForNewRun();

    bool IsBlocking() const { return m_active; }

    void EnqueueLines(const std::vector<std::string>& lines, Font& font, Shader& fontShader,
        std::function<void()> onSequenceComplete = nullptr);
    void EnqueueOpening(Font& font, Shader& fontShader);

    void Update(float dt, const Input::Input& input, Font& font, Shader& fontShader);
    void Draw(Font& font, Shader& textureShader, Shader& fontShader, const Math::Matrix& screenProjection);

private:
    void ClearLineTexture();
    void RebuildLineTexture(Font& font, Shader& fontShader);
    struct QueuedSequence
    {
        std::vector<std::string> lines;
        std::function<void()> onComplete;
        float preTypeDelay = 0.0f;
    };

    void BeginSequence(std::vector<std::string> lines, std::function<void()> onComplete, float preTypeDelay);
    void FinishSequence(Font& font, Shader& fontShader);

    std::unique_ptr<Background> m_boxImage;
    std::vector<std::string> m_lines;
    size_t m_lineIndex = 0;
    size_t m_visibleChars = 0;
    float m_charAccum = 0.0f;
    float m_secondsPerChar = 0.035f;
    float m_preTypeDelayRemaining = 0.0f;

    bool m_active = false;
    std::function<void()> m_onSequenceComplete;
    std::deque<QueuedSequence> m_pending;

    CachedTextureInfo m_lineTex{};
};

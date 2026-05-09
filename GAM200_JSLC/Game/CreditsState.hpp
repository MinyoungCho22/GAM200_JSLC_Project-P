//CreditsState.hpp

#pragma once
#include "../Engine/GameState.hpp"
#include "../Engine/Matrix.hpp"
#include "Font.hpp"
#include <memory>
#include <vector>

class GameStateManager;
class Shader;

class CreditsState : public GameState
{
public:
    explicit CreditsState(GameStateManager& gsm);
    void Initialize() override;
    void Update(double dt) override;
    void Draw() override;
    void Shutdown() override;

    bool BypassPostProcess() const override { return true; }

private:
    GameStateManager& gsm;

    // Solid-color shader for background and highlight bar
    std::unique_ptr<Shader> m_colorShader;
    unsigned int m_rectVAO = 0;
    unsigned int m_rectVBO = 0;

    // Font rendering
    std::unique_ptr<Shader> m_fontShader;
    std::unique_ptr<Font>   m_font;

    // Pre-baked text lines
    struct Line { CachedTextureInfo tex; float size; };
    std::vector<Line> m_lines;

    // Wait for the key that opened this screen to be released
    bool m_waitForRelease = false;

    // Helper
    void DrawRect(const Math::Matrix& proj,
                  float cx, float cy, float w, float h,
                  float r, float g, float b, float a);
};

//MainMenu.hpp

#pragma once
#include "../Engine/GameState.hpp"
#include "../Game/Font.hpp"
#include <memory>

class GameStateManager;
class Shader;

class MainMenu : public GameState
{
public:
    MainMenu(GameStateManager& gsm);
    void Initialize() override;
    void Update(double dt) override;
    void Draw() override;
    void Shutdown() override;

private:
    GameStateManager& gsm;

    std::unique_ptr<Font> m_font;
    std::unique_ptr<Shader> m_fontShader;

    CachedTextureInfo m_promptText;

    std::unique_ptr<Shader> m_shapeShader;
    unsigned int m_shapeVAO = 0;
    unsigned int m_shapeVBO = 0;

    double m_glitchTimer = 0.0;
    bool m_enterPressed = false;
    bool m_waitForEnterRelease = false;

    // Fade-out before launching gameplay
    bool  m_isFadingOut  = false;
    float m_fadeAlpha    = 0.0f;
    static constexpr float FADE_OUT_DURATION = 0.2f;
    unsigned int m_fadeVAO = 0;
    unsigned int m_fadeVBO = 0;
};
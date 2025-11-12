#pragma once
#include "../Engine/GameState.hpp"
#include "Font.hpp"
#include <memory>

class GameStateManager;
class Shader;

class GameOver : public GameState
{
public:
    GameOver(GameStateManager& gsm, bool& isGameOverFlag);
    void Initialize() override;
    void Update(double dt) override;
    void Draw() override;
    void Shutdown() override;

private:
    GameStateManager& gsm;
    bool& m_isGameOverFlag;
    bool m_isExiting = false;

    std::unique_ptr<Font> m_font;
    std::unique_ptr<Shader> m_fontShader;
    std::unique_ptr<Shader> m_colorShader;
    CachedTextureInfo m_gameOverText;
    CachedTextureInfo m_promptText;

    unsigned int m_quadVAO = 0;
    unsigned int m_quadVBO = 0;
};
#pragma once
#include "../Engine/GameState.hpp"
#include "Font.hpp"
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
    CachedTextureInfo m_titleText;
};
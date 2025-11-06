#pragma once
#include "../Engine/GameState.hpp"
#include <memory>

class GameStateManager;
class Shader;
class Font;

class SettingState : public GameState
{
public:
    SettingState(GameStateManager& gsm);
    void Initialize() override;
    void Update(double dt) override;
    void Draw() override;
    void Shutdown() override;

private:
    enum class MenuOption { Setting, Exit };

    GameStateManager& gsm;
    std::unique_ptr<Shader> m_colorShader;
    std::unique_ptr<Font> m_font;

    MenuOption m_currentSelection;
    unsigned int m_overlayVAO;
    unsigned int m_overlayVBO;
};
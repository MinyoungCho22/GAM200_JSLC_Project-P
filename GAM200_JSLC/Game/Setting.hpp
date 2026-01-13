// Setting.hpp

#pragma once
#include "../Engine/GameState.hpp"
#include <memory>
#include "Font.hpp" 
#include <vector> 

class GameStateManager;
class Shader;

/**
 * @class SettingState
 * @brief Manages the in-game settings menu, including display resolutions and application exit.
 */
class SettingState : public GameState
{
public:
    SettingState(GameStateManager& gsm);
    void Initialize() override;
    void Update(double dt) override;
    void Draw() override;
    void Shutdown() override;

private:
    // Menu navigation states
    enum class MenuPage { Main, Display };
    enum class MainOption { Resume, Exit };

    GameStateManager& gsm;
    std::unique_ptr<Shader> m_colorShader;
    std::unique_ptr<Font> m_font;
    std::unique_ptr<Shader> m_fontShader;

    MenuPage m_currentPage = MenuPage::Main;
    MainOption m_mainSelection = MainOption::Resume;
    int m_displaySelection = 0;

    // OpenGL objects for the background dimming overlay
    unsigned int m_overlayVAO;
    unsigned int m_overlayVBO;

    // Pre-baked UI text textures for performance
    CachedTextureInfo m_resumeText;
    CachedTextureInfo m_resumeSelectedText;
    CachedTextureInfo m_exitText;
    CachedTextureInfo m_exitSelectedText;

    CachedTextureInfo m_resRecommendedText; // e.g., "2560 x 1600 (Recommended)"
    CachedTextureInfo m_res1600Text;        // e.g., "1600 x 900"
};
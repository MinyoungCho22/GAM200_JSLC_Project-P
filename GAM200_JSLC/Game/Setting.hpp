#pragma once
#include "../Engine/GameState.hpp"
#include <memory>
#include "Font.hpp" 
#include <vector> 

class GameStateManager;
class Shader;

class SettingState : public GameState
{
public:
    SettingState(GameStateManager& gsm);
    void Initialize() override;
    void Update(double dt) override;
    void Draw() override;
    void Shutdown() override;

private:
    enum class MenuPage { Main, Display };
    enum class MainOption { Resume, Exit };

    GameStateManager& gsm;
    std::unique_ptr<Shader> m_colorShader;
    std::unique_ptr<Font> m_font;
    std::unique_ptr<Shader> m_fontShader;

    MenuPage m_currentPage = MenuPage::Main;
    MainOption m_mainSelection = MainOption::Resume;
    int m_displaySelection = 0;

    unsigned int m_overlayVAO;
    unsigned int m_overlayVBO;

    // 메인 메뉴
    CachedTextureInfo m_resumeText;
    CachedTextureInfo m_resumeSelectedText;
    CachedTextureInfo m_exitText;
    CachedTextureInfo m_exitSelectedText;

    // ✅ [수정] 디스플레이 메뉴 (Recommended 레이블이 텍스트에 포함됨)
    CachedTextureInfo m_resRecommendedText;     // "2560 x 1600 (Recommended)"
    CachedTextureInfo m_res1600Text;            // "1600 x 900"
    // CachedTextureInfo m_recommendedLabelText; // <-- 삭제됨
};
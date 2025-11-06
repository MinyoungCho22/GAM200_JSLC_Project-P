#pragma once
#include "../Engine/GameState.hpp"
#include <memory>

// ✅ [추가] Font.hpp 포함
#include "Font.hpp" 

class GameStateManager;
class Shader;
// class Font; // <--- 이제 Font.hpp에서 직접 포함됩니다.

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

    // ✅ [추가] 폰트 전용 셰이더
    std::unique_ptr<Shader> m_fontShader;

    MenuOption m_currentSelection;
    unsigned int m_overlayVAO;
    unsigned int m_overlayVBO;

    // ✅ [추가] FBO로 베이킹된 텍스트 텍스처
    CachedTextureInfo m_settingText;
    CachedTextureInfo m_settingSelectedText;
    CachedTextureInfo m_exitText;
    CachedTextureInfo m_exitSelectedText;
};
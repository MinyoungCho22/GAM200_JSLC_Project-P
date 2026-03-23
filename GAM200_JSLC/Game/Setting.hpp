// Setting.hpp

#pragma once
#include "../Engine/GameState.hpp"
#include "../Engine/Matrix.hpp"
#include <memory>
#include "Font.hpp"


class GameStateManager;
class Shader;

/**
 * @class SettingState
 * @brief In-game settings screen (ESC). Controls FPS cap, VSync, and master volume.
 */
class SettingState : public GameState
{
public:
    SettingState(GameStateManager& gsm);
    void Initialize() override;
    void Update(double dt) override;
    void Draw() override;
    void Shutdown() override;

    // Render settings as an opaque full-screen state.
    bool IsTransparent() const override { return false; }

    // Disable post-processing effects (exposure darkening) while settings UI is visible.
    bool BypassPostProcess() const override { return true; }

private:
    enum class MenuItem { FPS, VSync, Volume, Exit };

    static constexpr int FPS_COUNT = 5;
    static constexpr int FPS_VALUES[FPS_COUNT] = { 30, 60, 144, 240, 0 };
    static constexpr float VOLUME_STEP = 0.05f;

    GameStateManager& gsm;
    std::unique_ptr<Shader> m_colorShader;
    std::unique_ptr<Font>   m_font;
    std::unique_ptr<Shader> m_fontShader;

    // Current selections (synced with Engine/SoundSystem on Initialize)
    MenuItem m_selectedItem = MenuItem::FPS;
    int   m_fpsIndex     = 1;      // 0=30, 1=60, 2=144, 3=240, 4=No Limit
    bool  m_vsyncEnabled = false;
    float m_masterVolume = 0.8f;   // 0.0 to 1.0

    // OpenGL objects
    unsigned int m_overlayVAO = 0;
    unsigned int m_overlayVBO = 0;
    unsigned int m_barVAO    = 0;  // centered quad for bar drawing
    unsigned int m_barVBO    = 0;

    // Pre-baked static text
    CachedTextureInfo m_titleText;
    CachedTextureInfo m_fpsLabelText;
    CachedTextureInfo m_vsyncLabelText;
    CachedTextureInfo m_volumeLabelText;
    CachedTextureInfo m_exitText;
    CachedTextureInfo m_wasdHintText;
    CachedTextureInfo m_escHintText;

    // Dynamic value text (rebuilt when value changes)
    CachedTextureInfo m_fpsValueText;
    CachedTextureInfo m_vsyncValueText;
    CachedTextureInfo m_volumePctText;

    // Rebuild dynamic text strings after a value changes
    void RebuildValueTexts();

    // Draw a filled rectangle using m_colorShader (centered at cx, cy)
    void DrawRect(const Math::Matrix& proj, float cx, float cy,
                  float w, float h, float r, float g, float b, float a);

    // Apply current settings to Engine and SoundSystem
    void ApplyFps();
    void ApplyVSync();
    void ApplyVolume();

    const char* GetFpsLabel(int index) const;
};

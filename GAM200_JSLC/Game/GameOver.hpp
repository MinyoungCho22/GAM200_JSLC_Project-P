//GameOver.hpp

#pragma once
#include "../Engine/GameState.hpp"
#include "../Engine/Sound.hpp"
#include "../Game/Font.hpp"
#include <functional>
#include <memory>

class GameStateManager;
class Shader;

class GameOver : public GameState
{
public:
    /// onRespawn: called when player presses ENTER to respawn at checkpoint.
    /// If nullptr, pressing ENTER sets isGameOverFlag and pops back to gameplay (legacy behaviour).
    GameOver(GameStateManager& gsm_ref, bool& isGameOverFlag,
             std::function<void()> onRespawn = nullptr);
    void Initialize() override;
    void Update(double dt) override;
    void Draw() override;
    void Shutdown() override;
    bool BypassPostProcess() const override { return true; }

private:
    GameStateManager& gsm;
    bool& m_isGameOverFlag;
    std::function<void()> m_onRespawn;
    bool m_isExiting = false;

    double m_glitchTimer = 0.0;

    std::unique_ptr<Font> m_font;
    std::unique_ptr<Shader> m_fontShader;
    std::unique_ptr<Shader> m_colorShader;

    CachedTextureInfo m_promptText;

    unsigned int m_quadVAO = 0;
    unsigned int m_quadVBO = 0;

    float m_prevMasterVolume = 0.4f;
    bool  m_mutedOnEnter = false;
};
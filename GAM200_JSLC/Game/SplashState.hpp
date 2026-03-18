// SplashState.hpp

#pragma once
#include "../Engine/GameState.hpp"
#include "../Engine/Sound.hpp"
#include <memory>

class GameStateManager;
class Shader;

/**
 * @class SplashState
 * @brief DigiPen logo intro with runner-shadow effect and bloom fade-out.
 *
 * Phases (all driven by m_elapsed):
 *   [0,            T_FADE_IN)          Fade in  – logo rises from black
 *   [T_FADE_IN,    P_HOLD_START)       Wave 1   – girl, robot, drone run across (shadow)
 *   [P_HOLD_START, P_BLOOM_START)      Hold     – uniformly bright, silhouettes gone
 *   [P_BLOOM_START, TOTAL)             Bloom    – super-bright bloom burst + logo fades to black
 *                                                 → MainMenu shown immediately after
 *
 * Press Space / Enter / Escape to skip at any time.
 */
class SplashState : public GameState
{
public:
    explicit SplashState(GameStateManager& gsm);
    void Initialize() override;
    void Update(double dt) override;
    void Draw() override;
    void Shutdown() override;

private:
    GameStateManager& gsm;

    // ── timing ────────────────────────────────────────────────────────────
    static constexpr double T_FADE_IN   = 0.8;
    static constexpr double T_WAVE1     = 2.2;   // shadow-run phase
    static constexpr double T_HOLD      = 0.9;   // uniformly bright hold
    static constexpr double T_BLOOM_OUT = 0.5;   // bloom burst + fade to black
    static constexpr double TOTAL =
        T_FADE_IN + T_WAVE1 + T_HOLD + T_BLOOM_OUT;

    double m_elapsed     = 0.0;
    float  m_alpha       = 0.0f;
    float  m_brightFloor = 2.0f;
    float  m_shadowStr   = 1.0f;
    bool   m_skip        = false;

    // ── OpenGL ────────────────────────────────────────────────────────────
    std::unique_ptr<Shader> m_splashShader;
    std::unique_ptr<Shader> m_silhouetteShader;

    unsigned int m_VAO   = 0;
    unsigned int m_VBO   = 0;
    unsigned int m_texID = 0;
    int          m_texW  = 0;
    int          m_texH  = 0;

    // Player (girl) – 7-frame horizontal sprite sheet
    unsigned int m_playerTexID = 0;
    int          m_playerTexW  = 0;
    int          m_playerTexH  = 0;
    static constexpr int PLAYER_FRAMES = 7;

    // Robot
    unsigned int m_robotTexID = 0;
    int          m_robotTexW  = 0;
    int          m_robotTexH  = 0;

    // Drone
    unsigned int m_droneTexID = 0;
    int          m_droneTexW  = 0;
    int          m_droneTexH  = 0;

    // ── audio ─────────────────────────────────────────────────────────────
    Sound m_logoSound;
};

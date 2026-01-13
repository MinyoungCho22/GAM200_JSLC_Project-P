// SplashState.hpp

#pragma once
#include "../Engine/GameState.hpp"
#include "../Engine/Sound.hpp"
#include <memory>

class GameStateManager;
class Shader;

/**
 * @class SplashState
 * @brief Manages the initial logo screen sequence with fade-in, hold, and fade-out effects.
 */
class SplashState : public GameState
{
public:
    SplashState(GameStateManager& gsm);
    void Initialize() override;
    void Update(double dt) override;
    void Draw() override;
    void Shutdown() override;

private:
    GameStateManager& gsm;

    // Transition timing and transparency control
    double timer = 0.0;
    float currentAlpha = 0.0f;
    const double fadeInDuration = 1.0;
    const double holdDuration = 2.0;
    const double fadeOutDuration = 1.0;

    // OpenGL objects and texture metadata
    unsigned int VAO = 0;
    unsigned int VBO = 0;
    unsigned int textureID = 0;
    int imageWidth = 0;
    int imageHeight = 0;

    Sound logoSound;
};
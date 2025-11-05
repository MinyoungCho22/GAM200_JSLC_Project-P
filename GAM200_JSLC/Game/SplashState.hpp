#pragma once
#include "../Engine/GameState.hpp"
#include <memory>

class GameStateManager;
class Shader;

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
    std::unique_ptr<Shader> shader;

    double timer = 3.0; // 3초 타이머
    unsigned int VAO = 0;
    unsigned int VBO = 0;
    unsigned int textureID = 0;
    int imageWidth = 0;
    int imageHeight = 0;
};
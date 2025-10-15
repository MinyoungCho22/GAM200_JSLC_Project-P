#pragma once
#include "../Engine/GameState.hpp"
#include <memory>

class Shader;
class GameStateManager;

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
    double timer = 3.0;
    unsigned int VAO = 0, VBO = 0, textureID = 0;
    int imageWidth = 0, imageHeight = 0;
    std::unique_ptr<Shader> shader;
};
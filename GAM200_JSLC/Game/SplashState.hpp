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

    // ✅ [수정] 타이머를 3.0에서 2.0으로 변경
    double timer = 2.0;
    unsigned int VAO = 0;
    unsigned int VBO = 0;
    unsigned int textureID = 0;
    int imageWidth = 0;
    int imageHeight = 0;
};
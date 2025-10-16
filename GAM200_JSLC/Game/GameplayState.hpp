#pragma once
#include "../Engine/GameState.hpp"
#include "Player.hpp"
#include "Drone.hpp"
#include "PulseSource.hpp"
#include "PulseManager.hpp"
#include <memory>
#include <vector>

class Shader;
class GameStateManager;

class GameplayState : public GameState
{
public:
    GameplayState(GameStateManager& gsm);
    void Initialize() override;
    void Update(double dt) override;
    void Draw() override;
    void Shutdown() override;
private:
    GameStateManager& gsm;
    Player player;
    Drone drone;
    unsigned int groundVAO = 0, groundVBO = 0;
    std::unique_ptr<Shader> textureShader;
    std::unique_ptr<Shader> colorShader;
    std::vector<PulseSource> pulseSources;
    std::unique_ptr<PulseManager> pulseManager;
};
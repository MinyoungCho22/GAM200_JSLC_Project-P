#pragma once
#include "../Engine/GameState.hpp"
#include "../Engine/DebugRenderer.hpp"
#include "Player.hpp"
#include "PulseSource.hpp"
#include "PulseManager.hpp"
#include "DroneManager.hpp"
#include "PulseGauge.hpp"
#include "Room.hpp" // ✅ Background와 Camera 대신 Room을 포함
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
    // unsigned int groundVAO = 0, groundVBO = 0; // ◁ Room으로 이동 (제거)
    std::unique_ptr<Shader> textureShader;
    std::unique_ptr<Shader> colorShader;
    std::vector<PulseSource> pulseSources;
    std::unique_ptr<PulseManager> pulseManager;
    std::unique_ptr<DroneManager> droneManager;
    PulseGauge m_pulseGauge;
    std::unique_ptr<DebugRenderer> m_debugRenderer;
    bool m_isDebugDraw = false;
    // std::unique_ptr<Background> m_background; // ◁ Room으로 이동 (제거)
    std::unique_ptr<Room> m_room; // ✅ Room 객체 추가
    double m_logTimer = 0.0;
};
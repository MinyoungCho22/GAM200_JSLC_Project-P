#pragma once
#include "../Engine/GameState.hpp"
#include "../Engine/DebugRenderer.hpp"
#include "Player.hpp"
#include "PulseSource.hpp"
#include "PulseManager.hpp"
#include "DroneManager.hpp"
#include "PulseGauge.hpp"
#include "Room.hpp"
#include "Font.hpp"
#include "Setting.hpp"
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
    std::unique_ptr<Shader> textureShader; // Player, Drone, Background용
    std::unique_ptr<Shader> colorShader;   // Debug, UI용
    std::unique_ptr<Shader> m_fontShader;  // ✅ [추가] 폰트 전용 셰이더

    std::vector<PulseSource> pulseSources;
    std::unique_ptr<PulseManager> pulseManager;
    std::unique_ptr<DroneManager> droneManager;
    PulseGauge m_pulseGauge;
    std::unique_ptr<DebugRenderer> m_debugRenderer;
    bool m_isDebugDraw = false;
    std::unique_ptr<Room> m_room;
    double m_logTimer = 0.0;
    std::unique_ptr<Font> m_font;
};
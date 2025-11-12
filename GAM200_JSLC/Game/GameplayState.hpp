//GameplayState.hpp

#pragma once
#include "../Engine/GameState.hpp"
#include "../Engine/DebugRenderer.hpp"
#include "../Engine/Camera.hpp"
#include "Player.hpp"
#include "PulseSource.hpp"
#include "PulseManager.hpp"
#include "DroneManager.hpp"
#include "PulseGauge.hpp"
#include "Room.hpp"
#include "Font.hpp"
#include "Setting.hpp"
#include "Door.hpp"
#include "Hallway.hpp"
#include "Rooftop.hpp"
#include "TraceSystem.hpp" 
#include "GameOver.hpp" 
#include "MainMenu.hpp" 
#include <memory>
#include <vector>
#include <string>

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
    void HandleRoomToHallwayTransition();
    void HandleHallwayToRooftopTransition();

    GameStateManager& gsm;
    Player player;
    std::unique_ptr<Shader> textureShader;
    std::unique_ptr<Shader> colorShader;
    std::unique_ptr<Shader> m_fontShader;
    std::vector<PulseSource> pulseSources;
    std::unique_ptr<PulseManager> pulseManager;
    std::unique_ptr<DroneManager> droneManager;
    PulseGauge m_pulseGauge;
    std::unique_ptr<DebugRenderer> m_debugRenderer;
    bool m_isDebugDraw = false;
    std::unique_ptr<Room> m_room;
    std::unique_ptr<Door> m_door;
    std::unique_ptr<Door> m_rooftopDoor;
    double m_logTimer = 0.0;
    std::unique_ptr<Font> m_font;
    double m_fpsTimer = 0.0;
    int m_frameCount = 0;
    CachedTextureInfo m_fpsText;
    CachedTextureInfo m_pulseText;
    CachedTextureInfo m_debugToggleText;
    Camera m_camera;
    std::unique_ptr<Hallway> m_hallway;
    std::unique_ptr<Rooftop> m_rooftop;
    std::unique_ptr<TraceSystem> m_traceSystem;
    bool m_doorOpened = false;
    bool m_rooftopAccessed = false;
    bool m_isGameOver = false;

    float m_cameraSmoothSpeed = 0.1f;
};
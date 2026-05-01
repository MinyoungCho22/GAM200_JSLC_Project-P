// GameplayState.hpp

#pragma once
#include "../Engine/GameState.hpp"
#include "../Engine/DebugRenderer.hpp"
#include "../Engine/Camera.hpp"
#include "../Engine/Sound.hpp"
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
#include "MainMenu.hpp"
#include "Tutorial.hpp"
#include "StoryDialogue.hpp"
#include "Underground.hpp"
#include "Subway.hpp"
#include <memory>
#include <vector>

class Background;


class Shader;
class GameStateManager;

enum class MapZone { Room, Hallway, Rooftop, Underground, Subway };

enum class FadeState { None, FadingOut, FadingIn };
enum class PendingTransition { None, RoomToHallway, HallwayToRooftop, RooftopToUnderground, UndergroundToSubway };

class GameplayState : public GameState
{
public:
    GameplayState(GameStateManager& gsm);
    void Initialize() override;
    void Update(double dt) override;
    void Draw() override;
    void Shutdown() override;

    bool UsesLayeredDraw() const override { return true; }
    void DrawMainLayer() override;
    void DrawForegroundLayer(bool compositeToScreen = true) override;

private:
    void OpenHallwayDoorLayoutOnly();
    void HandleRoomToHallwayTransition();
    void RespawnAtCheckpoint();
    void StartTransition(PendingTransition t);
    void ExecutePendingTransition();
    void ApplyHallwayCameraBounds();
    void HandleHallwayToRooftopTransition();
    void HandleRooftopToUndergroundTransition();
    void HandleUndergroundToSubwayTransition();
    Math::Vec2 ScreenToWorldCoordinates(double screenX, double screenY) const;
    void WorldToFramebuffer(Math::Vec2 world, double& outFbX, double& outFbY) const;
    void ApplyGamepadDroneTargetingAssist(double dt, Input::Input& input, Math::Vec2& inOutMouseWorldPos);

    GameStateManager& gsm;
    Player player;
    std::unique_ptr<Shader> textureShader;
    std::unique_ptr<Shader> colorShader;
    std::unique_ptr<Shader> m_fontShader;
    std::unique_ptr<Shader> m_outlineShader;
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
    double m_fpsTimer = 0.0;
    int m_frameCount = 0;
    std::unique_ptr<Font> m_font;
    CachedTextureInfo m_pulseText;
    CachedTextureInfo m_debugToggleText;
    CachedTextureInfo m_fpsText;
    CachedTextureInfo m_warningLevelText;
    Camera m_camera;
    std::unique_ptr<Hallway> m_hallway;
    std::unique_ptr<Rooftop> m_rooftop;
    std::unique_ptr<TraceSystem> m_traceSystem;
    std::unique_ptr<Tutorial> m_tutorial;
    std::unique_ptr<StoryDialogue> m_storyDialogue;
    bool m_wasBlindOpen = false;
    bool m_roomBlindLowPulseStoryDone = false;
    /// After room->hall door opens: camera updates run, then this delay before hallway lines enqueue.
    bool m_hallwayEntryStoryPending = false;
    float m_hallwayEntryStoryDelayRemaining = 0.0f;
    /// Seconds to wait after gameplay starts before the opening story (player visible first).
    float m_openingStoryDelayRemaining = 0.0f;
    bool m_wasNearLiftRooftop = false;
    bool m_rooftopLiftStoryDone = false;
    bool m_hallwayFaradayBoxStoryDone = false;
    std::unique_ptr<Underground> m_underground;
    std::unique_ptr<Subway> m_subway;
    std::unique_ptr<Background> m_mouseLeftCursor;
    std::unique_ptr<Background> m_mouseRightCursor;
    std::unique_ptr<Background> m_hudFrame;
    std::unique_ptr<Background> m_hallwayHidingPromptS;
    Math::Vec2 m_lastMouseWorldPos{};
    bool m_undergroundAccessed = false;
    bool m_subwayAccessed = false;
    bool m_doorOpened = false;
    bool m_rooftopAccessed = false;
    bool m_isGameOver = false;
    MapZone m_currentCheckpoint = MapZone::Room;
    FadeState m_fadeState = FadeState::None;
    float m_fadeAlpha = 0.0f;
    PendingTransition m_pendingTransition = PendingTransition::None;
    unsigned int m_fadeVAO = 0;
    unsigned int m_fadeVBO = 0;
    static constexpr float FADE_OUT_DURATION = 0.1f;  // black until fully dark
    static constexpr float FADE_IN_DURATION  = 0.7f;   // slowly brightens back
    /// After any Ctrl+1..5 map cheat, ambient story lines stay off until a new game (Initialize).
    bool m_blockAmbientStoryForSession = false;
    float m_cameraSmoothSpeed = 0.1f;
    Sound m_bgm;

    // Q-skill: Pulse Resonance Burst (unlocked on Rooftop)
    float m_pulseDetonateCD = 0.0f;
    bool  m_pulseDetonateUnlocked = false;
    CachedTextureInfo m_pulseDetonateText{};

    // Subway zoom transition: camera zooms in when entering subway, then eases out
    float m_cameraZoom = 1.0f;
    bool m_subwayZoomTransition = false;
    float m_subwayZoomTimer = 0.0f;
    static constexpr float SUBWAY_ZOOM_DURATION = 3.0f;
    static constexpr float SUBWAY_ZOOM_START    = 2.8f; // strong zoom-in; eases out as player shrinks to 0.6
};
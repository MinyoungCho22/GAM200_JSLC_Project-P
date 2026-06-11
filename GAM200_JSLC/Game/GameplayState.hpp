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
#include "Train.hpp"
#include "Final.hpp"
#include "Skill.hpp"
#include "Background.hpp"
#include <memory>
#include <vector>

class Shader;
class GameStateManager;

enum class MapZone { Room, Hallway, Rooftop, Underground, Train, Final };

enum class FadeState { None, FadingOut, FadingIn };
enum class PendingTransition { None, RoomToHallway, HallwayToRooftop, RooftopToUnderground, UndergroundToTrain, TrainToFinal };

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
    void HandleUndergroundToTrainTransition();
    void HandleTrainToFinalTransition();
    Math::Vec2 ScreenToWorldCoordinates(double screenX, double screenY) const;
    void WorldToFramebuffer(Math::Vec2 world, double& outFbX, double& outFbY) const;
    void ApplyGamepadDroneTargetingAssist(double dt, Input::Input& input, Math::Vec2& inOutMouseWorldPos);
    TraceStage GetCurrentTraceStage() const;
    void RebuildTvLineTexture();
    void ResetTvNewsState();
    void ConfigureRoomTvPulseSource();
    // 커서가 TV 모니터 화면 위에 있고 플레이어가 TV 근처(가로)인지 — TV 좌클릭 켜기 판정용
    bool IsTvPowerHovered(Math::Vec2 playerHbCenter, Math::Vec2 mouseWorld) const;
    /// Boss_Say.png가 없거나 초기 로드에 실패했을 때 Final 진입 시 재시도
    void EnsureBossSayImageReady();

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
    std::unique_ptr<Train> m_train;
    std::unique_ptr<Final> m_final;
    std::unique_ptr<Background> m_mouseIdleCursor;
    std::unique_ptr<Background> m_mousePointerCursor;
    std::unique_ptr<Background> m_mouseLeftCursor;
    std::unique_ptr<Background> m_mouseRightCursor;
    std::unique_ptr<Background> m_hudFrame;
    std::unique_ptr<Background> m_conversionBackdrop;
    std::unique_ptr<Background> m_hallwayHidingPromptS;
    std::unique_ptr<Background> m_uiExplanation;
    bool m_showUiExplanation = false;
    bool m_uiExplanationSeen = false;
    std::unique_ptr<Background> m_scanlineDroneExplanation;
    bool m_showScanlineDroneExplanation = false;
    bool m_scanlineDroneExplanationSeen = false;
    Math::Vec2 m_lastMouseWorldPos{};
    bool m_undergroundAccessed = false;
    bool m_trainAccessed = false;
    bool m_finalAccessed = false;
    bool m_doorOpened = false;
    bool m_rooftopAccessed = false;
    bool m_isGameOver = false;
    float m_gameOverDelay = -1.0f;   ///< -1 = idle; >= 0 = 사망 후 GameOver 지연 카운트다운
    MapZone m_currentCheckpoint = MapZone::Room;
    bool m_checkpointTunnelInside = false;   ///< Train 체크포인트가 터널 인사이드인지 여부
    bool m_tunnelInsideEntryStoryDone = false; ///< 터널 인사이드 진입 컨텍스트 1회 표시 여부
    bool m_wasInTunnelInsideView = false;      ///< 직전 프레임 터널 인사이드 뷰 상태
    bool m_car5ReachedStoryDone = false;       ///< 물탱크 칸 도달 대사 1회 표시 여부
    bool m_wasCar5Encounter = false;           ///< 직전 프레임 Car5 조우 상태
    bool m_finalEntryStoryDone = false;        ///< Final 진입 보스 대사 1회 표시 여부
    std::unique_ptr<Background> m_bossSayImage; ///< Final 진입 시 페이드인되는 보스 대사 이미지
    bool m_bossSayActive = false;              ///< Boss_Say 페이드 시퀀스 진행 중
    bool m_bossSayDialogStarted = false;       ///< Boss_Say 다이얼로그가 시작된 적 있는지
    float m_bossSayAlpha = 0.0f;               ///< Boss_Say 현재 페이드 알파
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

    // SecondTrain(사이렌 칸) 진입 시 카메라 시네마틱: 오른쪽 가속 → 사이렌 드론 3초 노출 → 부드럽게 플레이어 복귀
    enum class SecondTrainCine { None, Pan, Hold, Return, Done };
    SecondTrainCine m_secondTrainCine        = SecondTrainCine::None;
    float           m_secondTrainCineTimer   = 0.0f;
    float           m_secondTrainCinePanSpeed = 0.0f;
    static constexpr float SECOND_TRAIN_CINE_HOLD_SEC = 3.0f;

    // Q-skill hint on first Rooftop visit — delayed StoryDialogue over Conversion.png
    PulseDetonateSkill m_pulseDetonateSkill;
    bool               m_prevRooftopForQHint  = false;
    bool               m_skipRooftopQHintByCheat = false;
    bool               m_rooftopQStoryDone = false;
    bool               m_rooftopQStoryPending = false;
    float              m_rooftopQStoryDelayRemaining = 0.0f;

    // Train zoom transition: camera zooms in when entering train map, then eases out
    float m_cameraZoom = 1.0f;
    bool m_trainZoomTransition = false;
    float m_trainZoomTimer = 0.0f;
    static constexpr float TRAIN_ZOOM_DURATION = 3.0f;
    static constexpr float TRAIN_ZOOM_START    = 2.8f; // strong zoom-in; eases out as player shrinks to 0.6
    /// Underground→Train 자연 진입: 페이드·카메라 줌이 끝난 뒤 출발 카운트다운 시작
    bool m_trainDeferEntryUntilIntroDone = false;
    float m_undergroundTrainBoardingDelay = -1.0f;

    Drone* m_lockedAttackDrone = nullptr;
    Robot* m_lockedAttackRobot = nullptr;
    float m_lockedAttackSide = 1.0f;
    float side = 1.0f;

    //story
    bool m_tvNewsActive = false;

    enum class TvPhase { OffGlitch, OnNews };
    TvPhase m_tvPhase = TvPhase::OffGlitch;

    std::vector<std::string> m_tvLeakLines = {
        "Bzzzt… kzzzt…",
        "...government...",
        "...searching for...",
        "...unidentified subject...",
    };
     
    std::vector<std::string> m_tvNewsLines = {
        "BREAKING: Authorities issue public warning.",
        "Government officials are searching",
        "for an unidentified individual.",
        "The subject is considered highly dangerous.",
        "If seen, do not approach. Report immediately.",
    };
    size_t m_tvLineIndex = 0;
    float  m_tvLineTimer = 0.0f;      
    float  m_tvFlickerTimer = 0.0f;   
    bool   m_tvTextVisible = true;   
    CachedTextureInfo m_tvLineTex{};

    // Ending Cutscene & Credits Roll (Final_3)
    bool m_isInFinal3Cutscene = false;
    enum class Final3State { None, FadingOutToFinal3, FadingInToFinal3, Walking, Credits, Exit };
    Final3State m_final3State = Final3State::None;
    float m_final3Timer = 0.0f;
    float m_creditsScrollY = 0.0f;
    std::unique_ptr<Background> m_final3Bg;
    struct CreditLine { CachedTextureInfo tex; float size; };
    std::vector<CreditLine> m_final3CreditsLines;
};
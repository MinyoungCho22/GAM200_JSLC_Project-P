// Train.hpp

#pragma once
#include "Background.hpp"
#include "PulseSource.hpp"
#include "DroneManager.hpp"
#include "Robot.hpp"
#include "../Engine/Sound.hpp"
#include "../Engine/Vec2.hpp"
#include <cstdint>
#include <array>
#include <memory>
#include <utility>
#include <vector>

namespace Math { class Matrix; }

class Shader;
class Player;
class DebugRenderer;
struct TrainObjectConfig;

enum class TrainState { Stationary, Starting, Moving };

/**
 * @class Train
 * @brief Train map - Final stage with train ride mechanics.
 *        Rail is static; train moves at camera speed for parallax effect.
 *        Five train cars (First…Third, Third_ThirdTrain, FourthTrain.png) assembled end-to-end.
 *        Sunset parallax sky background.
 */
class Train
{
public:
    // Map boundaries (WIDTH is sum of loaded car image widths — use GetMapWidth() at runtime)
    static constexpr float HEIGHT = 1080.0f;
    static constexpr float MIN_X  = 24180.0f;
    static constexpr float MIN_Y  = -3500.0f;

    // Train constants
    static constexpr float TRAIN_SPEED        = 280.0f; // max speed (world units per second)
    static constexpr float TRAIN_ACCEL        = 90.0f;  // acceleration (world units per second^2)
    static constexpr float TRAIN_DEPART_DELAY = 3.0f;   // seconds after map entry before departure

    /// Init 직후 드론 크기 기준(120×비율 등)에 곱하는 값 — 전투/추적 드론이 동일한 작은 실루엣을 쓰게
    static constexpr float kCombatDroneVisualScale = 0.75f;
    static void            ApplyCombatDroneVisualScale(Drone& d);

    struct Obstacle
    {
        Math::Vec2 pos;
        Math::Vec2 size;
    };

    enum class TrainHitboxKind : uint8_t
    {
        Solid,             // 전방향 AABB 충돌
        JumpThroughPipe    // 얇은 발판: 아래→위 점프 통과, 위에서만 착지, 웅크리면 낙하
    };

    // Hitbox in LOCAL train space:
    //   center.x = distance from train's initial left edge (MIN_X)
    //   center.y = distance from map bottom (MIN_Y), Y going up
    struct TrainHitbox
    {
        Math::Vec2 localCenter;
        Math::Vec2 size;
        bool collision = true; // false = 디버그 표시만, 물리 통과 (Third_Third 자동차 등)
        TrainHitboxKind kind = TrainHitboxKind::Solid;
    };

    // A hiding box moves with the train (same local-space coords as TrainHitbox).
    // When the player crouches inside one, drones cannot detect them.
    struct HidingSpot
    {
        Math::Vec2 localCenter;
        Math::Vec2 size;
    };

    /// Third_ThirdTrain 2×3 자동차 운반 구역 — 엔진 시동 / 펄스 주입 / 연쇄 반응
    struct CarTransportSlot
    {
        Math::Vec2 localCenter{};
        Math::Vec2 halfSize{};
        bool       engineOn       = false;
        /// 펄스 주입 누적량(0~kCarTransportPulseInjectTotal). 펄스 흡수가 끊기면 0으로 리셋.
        float      injectPulseAccum = 0.f;
        float      engineGlowTimer = 0.f;
        bool       skipPulseLineOverlay = false;
    };

    void Initialize();
    void ApplyConfig(const TrainObjectConfig& cfg);
    void Update(double dt, Player& player, Math::Vec2 playerHitboxSize,
                bool pulseAbsorbHeld, bool carTransportInjectHeld, bool ignoreCarInjectPulseCost,
                bool attackHeld, bool attackTriggered, Math::Vec2 mouseWorldPos,
                int carTransportInjectForcedSlot = -1);

    // Call once when the Train map becomes active to begin the departure countdown
    void StartEntryTimer();
    // Force reset to "first entered train" state (for checkpoint respawn on Train).
    void RestartEntryTimer();

    // Returns UI text for the departure countdown (empty string when not needed)
    std::string GetDepartureAnnouncementText() const;

    // textureShader: draws train car images + robots (레일 타일은 DrawRailTrack)
    // viewHalfW: half of currently visible world width (zoom-aware)
    void Draw(Shader& textureShader, Math::Vec2 cameraPos, float viewHalfW) const;

    /// rail.png 타일만 그림. 하늘(DrawBackground) 직후 호출해 다른 맵·차량보다 아래 레이어에 두는 용도.
    void DrawRailTrack(Shader& textureShader, Math::Vec2 cameraPos, float viewHalfW) const;

    // Draws sunset sky gradient bands (call before Draw, with colorShader active)
    // viewHalfW: half of currently visible world width (zoom-aware)
    void DrawBackground(Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW) const;

    // 시동된 차량 펄스 라이트(플레이스홀더). 열차 스프라이트 위에 그림.
    void DrawCarTransportVFX(Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW) const;
    // PulseLine / Start 아이콘 (텍스처). 열차 스프라이트에 이미 펄스가 있는 슬롯은 skipPulseLineOverlay로 스킵.
    void DrawCarTransportOverlays(Shader& textureShader, Math::Vec2 cameraPos, float viewHalfW) const;
    void DrawValveWaterVFX(Shader& colorShader, const Math::Matrix& worldProjection, Math::Vec2 cameraPos,
                           float viewHalfW) const;

    void DrawDrones(Shader& shader) const;
    void DrawRadars(const Shader& colorShader, DebugRenderer& debugRenderer) const;
    void DrawGauges(Shader& colorShader, DebugRenderer& debugRenderer) const;
    void DrawDebug(Shader& colorShader, DebugRenderer& debugRenderer) const;
    /// SecondTrain 보라 컨테이너 Enter / Leave 프롬프트 (월드 스페이스)
    void DrawCar2EnterLeavePrompt(Shader& textureShader, Math::Vec2 cameraPos, float viewHalfW) const;
    /// ThirdTrain 사이렌 파동 (solid_color)
    void DrawCar3SirenWaves(Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW) const;
    /// ThirdTrain 사이렌 펄스 차단 진행 — Room 충전소 남은 양 바와 같은 스타일, 사이렌 옆 월드 좌표 (solid_color)
    void DrawCar3SirenProgressGauge(Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW) const;
    /// Third_ThirdTrain 자동차 펄스 주입 진행(좌클릭 Start / 상호작용 키)
    void DrawCarTransportInjectProgressGauge(Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW) const;
    /// SecondTrain Enter/Leave 아이콘 위에 마우스가 있고 플레이어가 컨테이너 근처일 때 (커서 프롬프트용)
    bool IsCar2EnterLeavePromptHovered(Math::Vec2 playerHbCenter, Math::Vec2 playerHbSize, Math::Vec2 mouseWorld,
                                       Math::Vec2 cameraPos, float viewHalfW) const;
    /// ThirdTrain 사이렌: 근접 + 마우스가 사이렌 위 (좌클릭 펄스 주입 커서)
    bool IsCar3SirenMouseHoverForPulseInject(Math::Vec2 playerHbCenter, Math::Vec2 playerHbSize, Math::Vec2 mouseWorld) const;
    void Shutdown();

    const std::vector<Drone>& GetDrones() const;
    std::vector<Drone>& GetDrones();
    DroneManager* GetDroneManager() { return m_droneManager.get(); }
    DroneManager* GetSirenDroneManager() { return m_sirenDroneManager.get(); }
    /// Third_ThirdTrain 사이렌 전용 드론 (추적용 — FourthTrain 전투 드론과 분리)
    const std::vector<Drone>& GetSirenDrones() const;
    /// Third_Third 자동차 칸 주변 호버 드론 (Car5 전투 드론과 별도 매니저)
    DroneManager* GetCarTransportDroneManager() { return m_carTransportDroneManager.get(); }
    float GetCar3SirenInjectT() const { return m_car3SirenInjectT; }
    bool  GetCar3SirenActive() const { return m_car3SirenActive; }
    float GetCar2ContainerChargePct() const { return m_car2InsideCharge; }

    /// Car2 보라 펄스 박스 안(Inside) — 사이렌 드론 추적·공격 차단용
    bool IsPlayerInCar2PurplePulseBox(Math::Vec2 playerHbCenter, Math::Vec2 playerHitboxSize) const;
    bool IsSirenDroneDamageBlocked(Math::Vec2 playerHbCenter, Math::Vec2 playerHitboxSize, bool isPlayerCrouching) const;

    void ClearAllDrones();

    std::vector<PulseSource>& GetPulseSources() { return m_pulseSources; }
    const std::vector<Robot>& GetRobots() const  { return m_robots; }
    std::vector<Robot>& GetRobots()              { return m_robots; }

    // Returns true when the player is crouching inside a hiding spot (drones cannot see them).
    // playerHbCenter: 히트박스 중심 (GetPosition()이 아님)
    bool IsPlayerHiding(Math::Vec2 playerHbCenter, Math::Vec2 playerHitboxSize, bool isPlayerCrouching) const;
    const std::vector<HidingSpot>& GetHidingSpots() const { return m_hidingSpots; }

    /// 드론 피격 등으로 펄스 주입이 끊겼을 때 호출 — 해당 구역 주입 진행 리셋
    void NotifyCarTransportInjectionInterrupted();

    /// Q 스킬 등 근거리 펄스가 차량에 닿은 것처럼 처리할 월드 기준점 (범위 안일 때만 true)
    bool TryGetCarTransportSkillAnchor(Math::Vec2 playerHbCenter, Math::Vec2& outWorldAnchor) const;

    /// 시동 꺼진 차: Start 아이콘 위에 커서 + 플레이어가 차 슬롯 안에 있을 때 슬롯 인덱스 (펄스 주입 UI용)
    int TryGetCarTransportStartInjectSlot(Math::Vec2 playerHbCenter, Math::Vec2 playerHbSize,
                                          Math::Vec2 mouseWorldPos) const;

    /// 플레이어 히트박스와 차가 겹치고 마우스가 그 차 위에 있으며 시동 꺼진 경우 (마우스 커서용 / 클릭 시동)
    bool TryGetCarTransportClickIgniteTarget(Math::Vec2 playerHbCenter, Math::Vec2 playerHbSize,
                                             Math::Vec2 mouseWorldPos, int& outSlotIndex) const;

    /// 좌클릭 시동 (시동 이미 켜짐이면 false). 연쇄 반응 등 기존 로직 호출.
    bool TryIgniteCarTransportSlot(int slotIndex);
    void AppendCarTransportStraightChainArcs(std::vector<std::pair<Math::Vec2, Math::Vec2>>& outArcs) const;
    /// Q/펄스 시: 시동 중인 차 각각에서 가장 가까운 Car4 호버 드론으로 번가지(가지) 아크 추가
    void AppendCarTransportPulseBranchArcs(std::vector<std::pair<Math::Vec2, Math::Vec2>>& outArcs,
                                           Math::Vec2 pulseWorldCenter, float pulseRadius) const;
    /// Q 스킬: FourthTrain 전투 로봇에게 반경 내 넉백 + 데미지
    void ApplyPulseToTrainRobots(Math::Vec2 pulseWorldCenter, float radius);
    bool IsValveMouseHoverable(Math::Vec2 playerHbCenter, Math::Vec2 playerHbSize, Math::Vec2 mouseWorldPos) const;
    /// 커서용: 솔리드/파이프 열차 히트박스·정적 궤도 박스 위(상호작용 여부 무관)
    bool IsPointOverWorldCollisionAABB(Math::Vec2 worldPos, Math::Vec2 cursorHitboxSize) const;

    /// 플레이어 히트박스 X 기준 현재 칸 (1~5, 밖이면 0)
    int GetPlayerTrainCarIndex(Math::Vec2 worldHbCenter) const;
    /// 치트 텔레포트 등: 해당 호차(1~5) 중심 월드 X (열차 오프셋 반영)
    float GetTrainCarCenterWorldX(int carIndex1To5) const;
    /// 해당 칸 전투 드론·로봇(및 Car4 CT드론, Car3 사이렌드론) 전멸 여부
    bool IsTrainCarCombatCleared(int car1To5) const;
    /// 아직 클리어 안 된 가장 앞 칸의 경계 X (플레이어 최대 진행)
    float GetTrainCombatAdvanceCapWorldX() const;
    /// Car5 덱 최초 진입 시 밸브 힌트 (일반 진입만 — Alt 텔레포트 중엔 표시 안 함)
    std::string GetCar5ValveHintBannerText() const;

    void SetTrainCarCheatUnlock(bool v) { m_trainCheatCarUnlock = v; }

    void RequestTrainCameraShake(float maxPixelOffset);
    float ConsumeTrainCameraShakeRequest();

    void DrawRobotTrainAlerts(Shader& colorShader, DebugRenderer& debugRenderer) const;

    float      GetTrainOffset() const { return m_trainOffset; }
    /// 현재 열차 이동 속도(월드 유닛/초) — 추적 드론 보조 등에 사용
    float      GetTrainCurrentSpeed() const { return m_trainCurrentSpeed; }
    TrainState GetTrainState()  const { return m_trainState; }

    // Total world width of all train car images (1 px = 1 world unit)
    float GetMapWidth() const { return m_totalTrainWidth; }

    // Right boundary that expands as train moves (for camera bounds)
    float GetEffectiveRightBound() const { return MIN_X + m_totalTrainWidth + m_trainOffset + 960.0f; }

private:
    // Train car textures
    std::unique_ptr<Background> m_firstTrain;
    std::unique_ptr<Background> m_secondTrain;
    std::unique_ptr<Background> m_thirdTrain;
    std::unique_ptr<Background> m_thirdThirdTrain;
    std::unique_ptr<Background> m_fourthTrain;
    std::unique_ptr<Background> m_valveSprite;

    // Rail tile texture (tiled horizontally along train-map floor)
    std::unique_ptr<Background> m_railTile;

    Math::Vec2 m_position;
    Math::Vec2 m_size;

    // Rendered world width for each car (set from image pixel width at load time)
    float m_car1Width = 2640.0f;
    float m_car2Width = 2640.0f;
    float m_car3Width = 2640.0f;
    float m_car4Width = 2640.0f;
    float m_car5Width = 2640.0f;
    float m_totalTrainWidth = 7920.0f;

    // Rail tile info (computed after loading rail image)
    float m_railTileW  = 256.0f;
    float m_railTileH  = 69.0f;
    int   m_railTileCount = 40;

    // Train movement state
    TrainState m_trainState   = TrainState::Stationary;
    float      m_trainOffset  = 0.0f;
    float      m_trainCurrentSpeed = 0.0f;
    bool       m_playerOnTrain = false;
    Sound      m_trainStartSound;
    Sound      m_trainRunLoopSound;

    // Riding momentum: carry player X with moving train while grounded, jumping above cars, or crouching on deck
    bool       m_prevPlayerOnTrain = false;
    bool       m_airborneFromTrain = false;
    bool       m_crouchCarryLatched = false;
    bool       m_prevOnJumpThroughSurface = false;
    bool       m_prevCrouchHeld = false;     // edge-trigger pipe drop-through
    float      m_pipeDropCooldown = 0.0f;    // 드롭 발동 직후 잠시 파이프 충돌·스냅을 무시
    /// 칸 사이 갭: 덱 스냅 스킵 후 중력 낙하 → 레일 착지 시 펄스 소모
    bool       m_trainCarGapFalling = false;

    // Entry countdown: -1 means not yet started; counts up to TRAIN_DEPART_DELAY
    float m_entryTimer        = -1.0f;
    // How long to show the "train departed" message (brief flash after departure)
    float m_departedMsgTimer  = 0.0f;

    // Hitboxes defined in local train space
    std::vector<TrainHitbox> m_trainHitboxes;
    /// rail.png 등 월드 고정 발판 — localCenter = 절대 월드 중심(열차 m_trainOffset 없음)
    std::vector<TrainHitbox> m_staticWorldHitboxes;

    // Hiding spots (move with train, same local-space as TrainHitbox)
    std::vector<HidingSpot> m_hidingSpots;

    // Sky gradient geometry (filled quads via solid_color shader)
    unsigned int m_skyVAO = 0;
    unsigned int m_skyVBO = 0;

    // Config-driven obstacles / pulse sources (retain for JSON hot-reload)
    std::unique_ptr<DroneManager>  m_droneManager;
    std::unique_ptr<DroneManager>  m_sirenDroneManager;
    std::unique_ptr<DroneManager> m_carTransportDroneManager;
    std::vector<Obstacle>          m_obstacles;
    std::vector<PulseSource>       m_pulseSources;
    std::vector<Robot>             m_robots;

    void BuildTrainHitboxes();
    void InitSkyVAO();
    void DrawFilledQuad(Shader& colorShader,
                        Math::Vec2 center, Math::Vec2 size,
                        float r, float g, float b, float a = 1.0f) const;

    void InitValveWaterGpu();
    void ShutdownValveWaterGpu();
    void UploadAndDrawValveWaterGpu(const Math::Matrix& projection, Math::Vec2 cameraPos, float viewHalfW) const;
    void ApplyValveWaterDamageToEnemies(float dt);

    void ApplyTrainMotionToDronesAndRobots(float deltaTrainX);
    bool IsPlayerOnCar5Deck(Math::Vec2 hbCenter, Math::Vec2 hbSize) const;
    void TryActivateCar5Encounter(Math::Vec2 hbCenter, Math::Vec2 hbSize);
    void UpdateTrainEncounterScript(float dt, Player& player);
    void UpdateTrainRobotsAI(float dt, Player& player);
    void UpdateCar2PurpleContainer(float dt, Player& player, Math::Vec2 mouseWorldPos, bool attackTriggered,
                                   bool pulseAbsorbHeld, bool injectGodMode);
    void UpdateCar3Siren(float dt, Player& player, Math::Vec2 playerHbCenter, Math::Vec2 playerHitboxSize,
                         Math::Vec2 mouseWorldPos, bool attackHeld, bool injectGodMode);
    static void AssistRobotRailJumpTowardTrain(Robot& robot, const Player& player, float dt);
    float GetTrainCarLocalLeftEdge(int carIndex) const;
    void UpdateTrainDeckPatrolRobots(float dt, Player& player, Math::Vec2 playerHbCenter, Math::Vec2 playerHitboxSize);
    void TryDeckPatrolRobotJumpAndLandingShake(Robot& r, size_t robotIndex, float dt,
                                               const std::vector<ObstacleInfo>& tallCarObs, int playerTrainCar);
    void ClampCarSegment4RobotsBeforeValve();
    void ClampCar5RobotsEastOfValve();
    /// SecondTrain 보라 컨테이너 솔리드 히트박스와 동일한지 (숨기기 중 충돌 해제용)
    bool IsCar2PurpleHitbox(const TrainHitbox& hb) const;

    std::unique_ptr<Background> m_car2EnterPromptTex;
    std::unique_ptr<Background> m_car2LeavePromptTex;
    std::unique_ptr<Background> m_carTransportPulseLeftTex;
    std::unique_ptr<Background> m_carTransportPulseRightTex;
    std::unique_ptr<Background> m_carTransportStartTex;

    enum class Car2HidePhase : uint8_t { None, Entering, Inside };
    Car2HidePhase       m_car2HidePhase        = Car2HidePhase::None;
    float               m_car2HideSeqTime    = 0.f;
    float               m_car2InsideCharge     = 0.f;
    /// Enter 클릭 직전 위치(복귀용). 열차 이동분은 m_trainOffset 차로 보정.
    bool        m_car2EnterSavedValid = false;
    Math::Vec2  m_car2EnterPlayerWorld{};
    float       m_car2EnterTrainOffset  = 0.f;
    mutable bool        m_car2PromptHover      = false;

    TrainHitbox         m_car2PurpleHb{};
    bool                m_car2PurpleHbValid    = false;

    TrainHitbox         m_car3SirenHb{};
    bool                m_car3SirenHbValid       = false;
    bool                m_car3SirenActive        = true;
    float               m_car3SirenInjectT       = 0.f;
    float               m_car3SirenSpawnTimer    = 0.f;
    float               m_car3SirenWaveAnim      = 0.f;
    bool                m_car3SirenPendingShutdown = false;

    void ResetCarTransportSlotsToInitialState();
    void UpdateCarTransport(float dt, Player& player, Math::Vec2 playerHbCenter,
                            bool injectHeld, bool ignorePulseCost, int forcedInjectSlot = -1);
    void ApplyMooreConnectedPulseShare(Player& player, float dt);
    void FireStraightLineChainDetonations();
    void UpdateCarTransportDrones(float dt, const Player& player, Math::Vec2 playerHitboxSize, bool isPlayerHidingTrain);
    Math::Vec2 CarTransportWorldCenter(int slotIndex) const;
    int FindCarTransportInjectTarget(Math::Vec2 playerHbCenter) const;
    static bool IsMooreAdjacentCarSlots(int a, int b);
    void UpdateValveWaterParticles(float dt);

    std::array<CarTransportSlot, 6> m_carTransportSlots{};
    int                             m_carInjectFocusSlot = -1;

    // Car5 valve interaction + spill VFX
    Math::Vec2 m_valveLocalCenter{};
    Math::Vec2 m_valveVisualSize{ 135.0f, 135.0f };
    bool       m_valveDragging = false;
    float      m_valvePrevMouseAngle = 0.0f;
    float      m_valveOpenAccum = 0.0f;
    float      m_valveOpenT = 0.0f;
    float      m_valvePressureT = 0.0f;
    float      m_valveWaterAnimTime = 0.0f;
    float      m_valveParticleSpawnCarry = 0.0f;
    std::uint32_t m_valveParticleCounter = 0;

    struct ValveWaterParticle
    {
        Math::Vec2 pos{};
        Math::Vec2 vel{};
        Math::Vec2 size{};
        float life = 0.0f;
        float maxLife = 0.0f;
        float alpha = 0.0f;
    };
    std::vector<ValveWaterParticle> m_valveWaterParticles;
    /// 스플래시 스폰용(프레임마다 vector 재할당 방지)
    std::vector<ValveWaterParticle> m_valveSplashScratch;
    static constexpr std::size_t kMaxValveWaterParticles = 720;

    // GPU-instanced valve water rendering (single draw call; avoids CPU quad loops)
    struct ValveWaterGpuInstance
    {
        Math::Vec2 center{};
        Math::Vec2 halfSize{};
        float alpha = 0.0f;
        float layer = 0.0f; // 0 body, 1 core, 2 shadow
        float pad0 = 0.0f;
        float pad1 = 0.0f;
    };

    bool          m_valveWaterGpuReady = false;
    unsigned int  m_valveWaterProg = 0;
    int           m_valveWaterLocProjection = -1;
    unsigned int  m_valveWaterVAO = 0;
    unsigned int  m_valveWaterQuadVBO = 0;
    unsigned int  m_valveWaterInstVBO = 0;
    mutable std::uint32_t m_valveWaterInstPoolBytes = 0;
    mutable std::vector<ValveWaterGpuInstance> m_valveWaterGpuScratch;

    // FourthTrain (Car5 tank car): scripted drones / robots — idle until player steps on deck
    TrainHitbox       m_car5DeckHb{};
    bool              m_car5DeckHbValid = false;
    float             m_prevTrainOffsetActors = 0.0f;
    bool              m_car5EncounterActive = false;
    float             m_car5ValveHintTimer = 0.0f;
    bool              m_trainCheatCarUnlock        = false;
    float             m_encounterScriptTime = 0.0f;
    float             m_pendingTrainCameraShakePx = 0.f;
    std::vector<bool> m_trainDeckRobotWasAirborne;
    /// 덱 패트롤 로봇: 컨테이너 앞에서 점프하기까지 축적(초)
    std::vector<float> m_trainDeckRobotJumpPrepT;
    /// 덱 로봇: 첫 번째 컨테이너 점프 착지 시에만 카메라 쉐이크 1회
    std::vector<bool> m_trainDeckRobotUsedLandingShake;
    std::vector<float> m_droneWaterCd;
    std::vector<float> m_carTransportDroneWaterCd;
};

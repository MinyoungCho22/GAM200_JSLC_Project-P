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
        float      injectSeconds  = 0.f;
        float      engineGlowTimer = 0.f;
    };

    void Initialize();
    void ApplyConfig(const TrainObjectConfig& cfg);
    void Update(double dt, Player& player, Math::Vec2 playerHitboxSize,
                bool pulseAbsorbHeld, bool ignoreCarInjectPulseCost,
                bool attackHeld, bool attackTriggered, Math::Vec2 mouseWorldPos);

    // Call once when the Train map becomes active to begin the departure countdown
    void StartEntryTimer();
    // Force reset to "first entered train" state (for checkpoint respawn on Train).
    void RestartEntryTimer();

    // Returns UI text for the departure countdown (empty string when not needed)
    std::string GetDepartureAnnouncementText() const;

    // textureShader: draws rail tiles + train car images + robots
    // viewHalfW: half of currently visible world width (zoom-aware)
    void Draw(Shader& textureShader, Math::Vec2 cameraPos, float viewHalfW) const;

    // Draws sunset sky gradient bands (call before Draw, with colorShader active)
    // viewHalfW: half of currently visible world width (zoom-aware)
    void DrawBackground(Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW) const;

    // 시동된 차량 펄스 라이트(플레이스홀더). 열차 스프라이트 위에 그림.
    void DrawCarTransportVFX(Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW) const;
    void DrawValveWaterVFX(Shader& colorShader, const Math::Matrix& worldProjection, Math::Vec2 cameraPos,
                           float viewHalfW) const;

    void DrawDrones(Shader& shader) const;
    void DrawRadars(const Shader& colorShader, DebugRenderer& debugRenderer) const;
    void DrawGauges(Shader& colorShader, DebugRenderer& debugRenderer) const;
    void DrawDebug(Shader& colorShader, DebugRenderer& debugRenderer) const;
    void Shutdown();

    const std::vector<Drone>& GetDrones() const;
    std::vector<Drone>& GetDrones();
    DroneManager* GetDroneManager() { return m_droneManager.get(); }
    void ClearAllDrones();

    std::vector<PulseSource>& GetPulseSources() { return m_pulseSources; }
    const std::vector<Robot>& GetRobots() const  { return m_robots; }
    std::vector<Robot>& GetRobots()              { return m_robots; }

    // Returns true when the player is crouching inside a hiding spot (drones cannot see them)
    bool IsPlayerHiding(Math::Vec2 playerPos, Math::Vec2 playerHitboxSize, bool isPlayerCrouching) const;
    const std::vector<HidingSpot>& GetHidingSpots() const { return m_hidingSpots; }

    /// 드론 피격 등으로 펄스 주입이 끊겼을 때 호출 — 해당 구역 주입 진행 리셋
    void NotifyCarTransportInjectionInterrupted();

    /// Q 스킬 등 근거리 펄스가 차량에 닿은 것처럼 처리할 월드 기준점 (범위 안일 때만 true)
    bool TryGetCarTransportSkillAnchor(Math::Vec2 playerHbCenter, Math::Vec2& outWorldAnchor) const;

    /// 플레이어 히트박스와 차가 겹치고 마우스가 그 차 위에 있으며 시동 꺼진 경우 (마우스 커서용 / 클릭 시동)
    bool TryGetCarTransportClickIgniteTarget(Math::Vec2 playerHbCenter, Math::Vec2 playerHbSize,
                                             Math::Vec2 mouseWorldPos, int& outSlotIndex) const;

    /// 좌클릭 시동 (시동 이미 켜짐이면 false). 연쇄 반응 등 기존 로직 호출.
    bool TryIgniteCarTransportSlot(int slotIndex);
    void AppendCarTransportStraightChainArcs(std::vector<std::pair<Math::Vec2, Math::Vec2>>& outArcs) const;
    bool IsValveMouseHoverable(Math::Vec2 playerHbCenter, Math::Vec2 playerHbSize, Math::Vec2 mouseWorldPos) const;

    float      GetTrainOffset() const { return m_trainOffset; }
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

    // Entry countdown: -1 means not yet started; counts up to TRAIN_DEPART_DELAY
    float m_entryTimer        = -1.0f;
    // How long to show the "train departed" message (brief flash after departure)
    float m_departedMsgTimer  = 0.0f;

    // Hitboxes defined in local train space
    std::vector<TrainHitbox> m_trainHitboxes;

    // Hiding spots (move with train, same local-space as TrainHitbox)
    std::vector<HidingSpot> m_hidingSpots;

    // Sky gradient geometry (filled quads via solid_color shader)
    unsigned int m_skyVAO = 0;
    unsigned int m_skyVBO = 0;

    // Config-driven obstacles / pulse sources (retain for JSON hot-reload)
    std::unique_ptr<DroneManager>  m_droneManager;
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

    void ResetCarTransportSlotsToInitialState();
    void UpdateCarTransport(float dt, Player& player, Math::Vec2 playerHbCenter,
                            bool pulseAbsorbHeld, bool ignorePulseCost);
    void ApplyMooreConnectedPulseShare(Player& player, float dt);
    void FireStraightLineChainDetonations();
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
};

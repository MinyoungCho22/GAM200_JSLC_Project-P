//Underground.hpp

#pragma once
#include "../Engine/Vec2.hpp"
#include "../Game/PulseSource.hpp"
#include "Background.hpp"
#include "Robot.hpp"
#include <memory>
#include <vector>
class Shader;
class Player;
class DroneManager;
class Drone;
class DebugRenderer;
struct UndergroundObjectConfig;

class Underground
{
public:
    static constexpr float DEFAULT_WIDTH  = 7920.0f;
    static constexpr float HEIGHT       = 1080.0f;
    static constexpr float MIN_X        = 16260.0f;
    static constexpr float MIN_Y        = -2000.0f;
    /// Legacy alias — prefer GetMapWidth() after Initialize().
    static constexpr float WIDTH = DEFAULT_WIDTH;

    float GetMapWidth() const { return m_mapWidth; }
    float GetMapHeight() const { return m_mapHeight; }
    /// Player world X must exceed this to board the train (right of vending machine).
    float GetTrainBoardingMinWorldX() const { return m_trainBoardingMinWorldX; }
    bool IsApproachTrainDocked() const { return m_approachTrainDocked; }
    bool IsPlayerOnApproachTrain(Math::Vec2 playerHbCenter, Math::Vec2 playerHitboxSize) const;

    struct Obstacle
    {
        Math::Vec2 pos;
        Math::Vec2 size;
        std::unique_ptr<Background> sprite;
    };

    struct Ramp
    {
        Math::Vec2 pos;
        Math::Vec2 size;
        bool isLeftLow;
    };

    struct LightOverlay
    {
        Math::Vec2 pos;
        Math::Vec2 size;
        std::unique_ptr<Background> sprite;
    };

    void Initialize();
    /// Entry tracer is drone index 0; live_drone_states.json overwrites HP at startup—call after ApplyLiveStateToDrone.
    void ReapplyEntryTracerDroneAfterLiveState();
    void ApplyConfig(const UndergroundObjectConfig& cfg);
    void Update(double dt, Player& player, Math::Vec2 playerHitboxSize);
    void Draw(Shader& shader) const;
    /// Train 맵과 동일한 석양·구름 패럴랙스 하늘 (맵 스프라이트보다 먼저 그림)
    void DrawParallaxBackground(Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW) const;
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
    void RefillPulseSourcesAfterCheckpointRespawn();
    /// 게임 오버·체크포인트 복귀 시 연출 열차를 맵 밖 대기 상태로 되돌림
    void ResetApproachTrainMotion();

    const std::vector<Robot>& GetRobots() const { return m_robots; }
    std::vector<Robot>& GetRobots() { return m_robots; }

    /// Q 펄스: 반경 내 스토커 로봇 넉백 + 데미지 (열차 맵과 동일 논리)
    void ApplyPulseToRobots(Math::Vec2 pulseWorldCenter, float radius);

    bool IsPlayerHiding(Math::Vec2 playerHbCenter, Math::Vec2 playerHitboxSize, bool isPlayerCrouching) const;
    /// 커서용: JSON 장애물·라이트·램프 위(상호작용 없어도 오브젝트로 간주)
    bool IsPointOverConfiguredGeometry(Math::Vec2 worldPos, Math::Vec2 cursorHitboxSize) const;

private:
    std::unique_ptr<Background> m_background;
    std::unique_ptr<Background> m_approachTrain;
    Math::Vec2 m_position;
    Math::Vec2 m_size;
    float m_mapWidth = DEFAULT_WIDTH;
    float m_mapHeight = HEIGHT;
    float m_trainBoardingMinWorldX = MIN_X + DEFAULT_WIDTH;
    float m_approachTrainBlend = 0.0f;
    float m_approachTrainCenterX = 0.0f;
    float m_approachTrainCenterY = MIN_Y + HEIGHT * 0.5f;
    float m_approachTrainWidth = 0.0f;
    float m_approachTrainHeight = 0.0f;
    float m_approachTrainTargetCenterX = 0.0f;
    float m_approachTrainHiddenCenterX = 0.0f;
    bool m_approachTrainTriggered = false;
    bool m_approachTrainDocked = false;
    float m_approachTrainVelX = 0.0f;
    std::unique_ptr<DroneManager> m_droneManager;

    std::vector<Obstacle> m_obstacles;
    std::vector<LightOverlay> m_lights;
    std::vector<Ramp> m_ramps;
    std::vector<PulseSource> m_pulseSources;
    std::vector<Robot> m_robots;

    struct HidingVolume
    {
        Math::Vec2 center{};
        Math::Vec2 size{};
    };
    std::vector<HidingVolume> m_hidingSpots;

    void RecalculateApproachTrainAnchors();

    void InitParallaxSkyVAO();
    void DrawFilledQuad(Shader& colorShader, Math::Vec2 center, Math::Vec2 size, float r, float g, float b,
                        float a = 1.0f) const;

    unsigned int m_parallaxSkyVAO = 0;
    unsigned int m_parallaxSkyVBO = 0;
};
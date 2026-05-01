// Train.hpp

#pragma once
#include "Background.hpp"
#include "PulseSource.hpp"
#include "DroneManager.hpp"
#include "Robot.hpp"
#include "../Engine/Vec2.hpp"
#include <memory>
#include <vector>

class Shader;
class Player;
class DebugRenderer;
struct TrainObjectConfig;

enum class TrainState { Stationary, Starting, Moving };

/**
 * @class Train
 * @brief Train map - Final stage with train ride mechanics.
 *        Rail is static; train moves at camera speed for parallax effect.
 *        Three train cars (FirstTrain, SecondTrain, ThirdTrain) assembled end-to-end.
 *        Sunset parallax sky background.
 */
class Train
{
public:
    // Map boundaries
    static constexpr float WIDTH  = 7920.0f;
    static constexpr float HEIGHT = 1080.0f;
    static constexpr float MIN_X  = 24180.0f;
    static constexpr float MIN_Y  = -3500.0f;

    // Train constants
    static constexpr float TRAIN_SPEED        = 280.0f; // world units per second
    static constexpr float TRAIN_DEPART_DELAY = 3.0f;   // seconds after map entry before departure

    struct Obstacle
    {
        Math::Vec2 pos;
        Math::Vec2 size;
    };

    // Hitbox in LOCAL train space:
    //   center.x = distance from train's initial left edge (MIN_X)
    //   center.y = distance from map bottom (MIN_Y), Y going up
    struct TrainHitbox
    {
        Math::Vec2 localCenter;
        Math::Vec2 size;
    };

    // A hiding box moves with the train (same local-space coords as TrainHitbox).
    // When the player crouches inside one, drones cannot detect them.
    struct HidingSpot
    {
        Math::Vec2 localCenter;
        Math::Vec2 size;
    };

    void Initialize();
    void ApplyConfig(const TrainObjectConfig& cfg);
    void Update(double dt, Player& player, Math::Vec2 playerHitboxSize);

    // Call once when the Train map becomes active to begin the departure countdown
    void StartEntryTimer();

    // Returns UI text for the departure countdown (empty string when not needed)
    std::string GetDepartureAnnouncementText() const;

    // textureShader: draws rail tiles + train car images + robots
    // viewHalfW: half of currently visible world width (zoom-aware)
    void Draw(Shader& textureShader, Math::Vec2 cameraPos, float viewHalfW) const;

    // Draws sunset sky gradient bands (call before Draw, with colorShader active)
    // viewHalfW: half of currently visible world width (zoom-aware)
    void DrawBackground(Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW) const;

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

    float      GetTrainOffset() const { return m_trainOffset; }
    TrainState GetTrainState()  const { return m_trainState; }

    // Right boundary that expands as train moves (for camera bounds)
    float GetEffectiveRightBound() const { return MIN_X + WIDTH + m_trainOffset + 960.0f; }

private:
    // Train car textures
    std::unique_ptr<Background> m_firstTrain;
    std::unique_ptr<Background> m_secondTrain;
    std::unique_ptr<Background> m_thirdTrain;

    // Rail tile texture (tiled horizontally along train-map floor)
    std::unique_ptr<Background> m_railTile;

    Math::Vec2 m_position;
    Math::Vec2 m_size;

    // Rendered world width for each car (set from image pixel width at load time)
    float m_car1Width = 2640.0f;
    float m_car2Width = 2640.0f;
    float m_car3Width = 2640.0f;

    // Rail tile info (computed after loading rail image)
    float m_railTileW  = 256.0f;
    float m_railTileH  = 69.0f;
    int   m_railTileCount = 40;

    // Train movement state
    TrainState m_trainState   = TrainState::Stationary;
    float      m_trainOffset  = 0.0f;
    bool       m_playerOnTrain = false;

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
};

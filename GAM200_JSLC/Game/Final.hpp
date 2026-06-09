#pragma once
#include "../Engine/Vec2.hpp"
#include "Background.hpp"
#include "PulseSource.hpp"
#include "Player.hpp"
#include <memory>
#include <vector>

class Shader;
class DebugRenderer;

class Final
{
public:
    static constexpr float MIN_X  = 50000.0f;
    static constexpr float MIN_Y  = -2000.0f;
    static constexpr float HEIGHT = 1080.0f;

    struct Hitbox
    {
        Math::Vec2 pos;
        Math::Vec2 size;
        bool isYellow; // true = yellow/tall block, false = orange/low slab
    };

    struct FloorSweep
    {
        float x;
        float speed;
        float width;
        bool damagedPlayer;
    };

    void Initialize();
    void Update(double dt, Player& player, Math::Vec2 playerHitboxSize);
    void Draw(Shader& shader, Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW, const Math::Matrix& projection);
    void DrawDebug(Shader& colorShader, DebugRenderer& debugRenderer) const;
    void DrawPulseVents(Shader& shader, Shader& outlineShader, Math::Vec2 cameraPos, float viewHalfW);
    void Shutdown();

    float GetMapWidth() const { return m_mapWidth; }
    float GetMapHeight() const { return HEIGHT; }

    const std::vector<Hitbox>& GetHitboxes() const { return m_hitboxes; }
    std::vector<PulseSource>& GetPulseSources() { return m_pulseSources; }
    const std::vector<PulseSource>& GetPulseSources() const { return m_pulseSources; }
    int GetActiveVentIndex() const { return m_activeVentIndex; }

private:
    void InitSkyVAO();
    void DrawFilledQuad(Shader& colorShader, Math::Vec2 center, Math::Vec2 size,
                        float r, float g, float b, float a = 1.0f) const;
    void DrawParallaxBackground(Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW) const;
    void DrawParallaxLayer(Shader& shader, Background& bg, Math::Vec2 cameraPos, float viewHalfW, float scrollSpeedFactor);

    std::unique_ptr<Background> m_final1;
    std::unique_ptr<Background> m_final2;
    std::unique_ptr<Background> m_cityLast;
    std::unique_ptr<Background> m_cityMiddle;
    std::unique_ptr<Background> m_cityFront;
    std::unique_ptr<Background> m_pulseLine;

    float m_mapWidth = 7920.0f;
    std::vector<Hitbox> m_hitboxes;
    std::vector<FloorSweep> m_sweeps;
    float m_sweepSpawnTimer = 0.0f;
    bool m_spawnDirectionAlternate = false;

    unsigned int m_skyVAO = 0;
    unsigned int m_skyVBO = 0;

    std::vector<PulseSource> m_pulseSources;
    std::unique_ptr<Background> m_pulseVentSprite;

    int m_activeVentIndex = -1;
    float m_ventTimer = 0.0f;
    static constexpr float VENT_ACTIVE_DURATION = 5.0f;
    static constexpr float VENT_CYCLE_DURATION = 8.0f;

    Player m_boss;
};

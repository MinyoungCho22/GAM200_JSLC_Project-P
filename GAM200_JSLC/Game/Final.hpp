// Final.hpp

#pragma once
#include "../Engine/Vec2.hpp"
#include "Background.hpp"
#include <memory>

class Shader;
class Player;

class Final
{
public:
    static constexpr float MIN_X  = 50000.0f;
    static constexpr float MIN_Y  = -2000.0f;
    static constexpr float HEIGHT = 1080.0f;

    void Initialize();
    void Update(double dt, Player& player, Math::Vec2 playerHitboxSize);
    void Draw(Shader& shader, Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW);
    void Shutdown();

    float GetMapWidth() const { return m_mapWidth; }
    float GetMapHeight() const { return HEIGHT; }

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

    float m_mapWidth = 7920.0f;

    unsigned int m_skyVAO = 0;
    unsigned int m_skyVBO = 0;
};

//PulseSource.hpp

#pragma once
#include "../Engine/Vec2.hpp"
#include <memory>

class Shader;
class Background;

class PulseSource
{
public:
    void Initialize(Math::Vec2 position, Math::Vec2 size, float initial_pulse);

    void SetHitboxMargin(float margin) { m_hitboxMargin = margin; }
    void SetDrawRemainGauge(bool show) { m_drawRemainGauge = show; }

    void SharePulseStorageWith(PulseSource& leader);

    // texPath: PNG asset path. pivot (0.5, 0.5) means center-based (default).
    void InitializeSprite(const char* texPath, Math::Vec2 pivot = { 0.5f, 0.5f });

    float Drain(float amount);

    void Draw(Shader& shader) const;
    void DrawSprite(Shader& textureShader) const;
    void DrawOutline(Shader& outlineShader) const;

    void DrawRemainGauge(Shader& colorShader) const;

    void Shutdown();

    Math::Vec2 GetPosition() const { return m_position; }
    Math::Vec2 GetSize() const { return m_size; }
    Math::Vec2 GetHitboxSize() const
    {
        return { m_size.x + 2.0f * m_hitboxMargin, m_size.y + 2.0f * m_hitboxMargin };
    }

    bool HasPulse() const { return *m_sharedCurrent > 0.0f; }
    bool HasSprite() const { return m_sprite != nullptr; }

private:
    Math::Vec2 m_position{};
    Math::Vec2 m_size{};
    Math::Vec2 m_pivot{ 0.5f, 0.5f };
    float m_hitboxMargin = 0.0f;
    bool m_drawRemainGauge = true;

    std::shared_ptr<float> m_sharedCurrent;
    std::shared_ptr<float> m_sharedMax;
    std::shared_ptr<bool> m_sharedGaugeUnlocked;

    std::unique_ptr<Background> m_sprite;

    unsigned int VAO = 0;
    unsigned int VBO = 0;
};

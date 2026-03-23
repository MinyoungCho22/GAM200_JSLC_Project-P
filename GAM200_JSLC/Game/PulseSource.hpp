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

    // texPath: PNG asset path. pivot (0.5, 0.5) means center-based (default).
    void InitializeSprite(const char* texPath, Math::Vec2 pivot = { 0.5f, 0.5f });

    float Drain(float amount);

    void Draw(Shader& shader) const;              // Legacy solid-color quad (fallback)
    void DrawSprite(Shader& textureShader) const; // PNG sprite rendering
    void DrawOutline(Shader& outlineShader) const; // Pixel-outline rendering

    void Shutdown();

    Math::Vec2 GetPosition() const { return m_position; }
    Math::Vec2 GetSize()     const { return m_size; }
    bool HasPulse()          const { return m_current_pulse > 0; }
    bool HasSprite()         const { return m_sprite != nullptr; }

private:
    Math::Vec2 m_position{};
    Math::Vec2 m_size{};
    Math::Vec2 m_pivot{ 0.5f, 0.5f };

    float m_max_pulse    = 0.0f;
    float m_current_pulse = 0.0f;

    std::unique_ptr<Background> m_sprite; // PNG sprite (nullptr when absent)

    unsigned int VAO = 0;
    unsigned int VBO = 0;
};
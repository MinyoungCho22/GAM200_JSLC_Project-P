#pragma once
#include "../Engine/Vec2.hpp"

class Shader;

class PulseSource
{
public:
    void Initialize(Math::Vec2 position, Math::Vec2 size, float initial_pulse);
    float Drain(float amount);
    void Draw(Shader& shader) const;
    void Shutdown();

    Math::Vec2 GetPosition() const { return m_position; }
    Math::Vec2 GetSize() const { return m_size; }
    bool HasPulse() const { return m_current_pulse > 0; }

private:
    
    Math::Vec2 m_position{};
    Math::Vec2 m_size{};

    float m_max_pulse = 0.0f;
    float m_current_pulse = 0.0f;

    unsigned int VAO = 0;
    unsigned int VBO = 0;
};
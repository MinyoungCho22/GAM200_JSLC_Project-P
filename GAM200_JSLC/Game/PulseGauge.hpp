//PulseGauge.hpp

#pragma once
#include "../Engine/Vec2.hpp"

class Shader;

class PulseGauge
{
public:

    void Initialize(Math::Vec2 position, Math::Vec2 size);
    void Update(float current_pulse, float max_pulse);
    void Draw(Shader& shader);
    void Shutdown();

private:

    Math::Vec2 m_position;
    Math::Vec2 m_size;
    float m_pulse_ratio = 0.2f;

    unsigned int VAO = 0;
    unsigned int VBO = 0;
};
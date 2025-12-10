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

    Math::Vec2 m_position; // 게이지 바의 위치 (중심점)
    Math::Vec2 m_size;     // 게이지 바의 전체 크기
    float m_pulse_ratio = 0.2f; // 펄스 비율 (0.0 ~ 1.0)

    // 렌더링용
    unsigned int VAO = 0;
    unsigned int VBO = 0;
};
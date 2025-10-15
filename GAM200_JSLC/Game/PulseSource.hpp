#pragma once
#include "../Engine/Vec2.hpp"

class Shader;

class PulseSource
{
public:
    void Initialize(Math::Vec2 position, Math::Vec2 size, float initial_pulse);

    // 펄스를 소모시키고, 실제로 소모된 양을 반환합니다.
    float Drain(float amount);

    void Draw(Shader& shader) const;
    void Shutdown();

    // 충돌 감지 및 상태 확인을 위한 함수들
    Math::Vec2 GetPosition() const { return m_position; }
    Math::Vec2 GetSize() const { return m_size; }
    bool HasPulse() const { return m_current_pulse > 0; }

private:
    Math::Vec2 m_position;
    Math::Vec2 m_size;

    float m_max_pulse;
    float m_current_pulse;

    // 렌더링을 위한 VAO/VBO
    unsigned int VAO = 0;
    unsigned int VBO = 0;
};
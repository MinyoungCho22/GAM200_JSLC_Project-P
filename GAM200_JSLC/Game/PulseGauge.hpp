#pragma once
#include "../Engine/Vec2.hpp"

class Shader;

class PulseGauge
{
public:
    // 게이지 바의 위치, 크기를 설정하고 렌더링 리소스를 초기화합니다.
    void Initialize(Math::Vec2 position, Math::Vec2 size);

    // 현재 펄스 값과 최대 펄스 값을 받아 내부 상태를 업데이트합니다.
    void Update(float current_pulse, float max_pulse);

    // 게이지 바를 화면에 그립니다.
    void Draw(Shader& shader);

    // 사용한 리소스를 해제합니다.
    void Shutdown();

private:
    Math::Vec2 m_position; // 게이지 바의 위치 (중심점)
    Math::Vec2 m_size;     // 게이지 바의 전체 크기
    float m_pulse_ratio = 1.0f; // 펄스 비율 (0.0 ~ 1.0)

    // 렌더링용
    unsigned int VAO = 0;
    unsigned int VBO = 0;
};
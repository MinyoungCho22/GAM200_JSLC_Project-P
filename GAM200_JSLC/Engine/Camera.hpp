#pragma once
#include "Vec2.hpp"
#include "Matrix.hpp"
#include "Rect.hpp"

class Camera
{
public:
    Camera(Math::Rect player_zone);

    // 카메라의 위치를 즉시 설정
    void SetPosition(Math::Vec2 new_position);
    // 카메라의 현재 위치를 반환
    [[nodiscard]] const Math::Vec2& GetPosition() const;

    // 카메라가 움직일 수 있는 월드의 경계를 설정
    void SetLimit(Math::Rect new_limit);
    // 카메라의 현재 경계를 반환
    [[nodiscard]] const Math::Rect& GetLimit() const;

    // 플레이어가 자유롭게 움직일 수 있는 화면 내 영역을 설정
    void SetZone(Math::Rect new_zone);

    // 렌더링에 사용할 뷰(view) 행렬을 계산하여 반환
    [[nodiscard]] Math::Matrix GetViewMatrix() const;

    // 지정된 시간과 강도로 카메라를 흔들기
    void Shake(float duration, float magnitude);

    // 지정된 위치로 카메라를 즉시 이동 (경계 제한 적용).
    void CenterOn(Math::Vec2 target_position);

    // 매 프레임 호출되어 플레이어 위치를 따라 카메라를 업데이트
    void Update(const Math::Vec2& player_position, double dt);

private:
    Math::Rect m_limit;
    Math::Vec2 m_position{ 0.0f, 0.0f };
    Math::Rect m_player_zone;

    // 부드러운 이동을 위한 변수
    float m_smoothing = 15.0f;

    // 카메라 쉐이크를 위한 변수
    float m_shake_timer = 0.0f;
    float m_shake_magnitude = 0.0f;
};
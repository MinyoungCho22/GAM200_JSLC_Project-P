// Camera.hpp

#pragma once
#include "Vec2.hpp"
#include "Matrix.hpp"

class Camera
{
public:
    Camera();

    void Initialize(Math::Vec2 position, float viewWidth, float viewHeight);
    void Update(Math::Vec2 targetPosition, float smoothSpeed = 0.1f);
    void SetPosition(Math::Vec2 position);
    void SetBounds(Math::Vec2 minBounds, Math::Vec2 maxBounds);

    // Camera animation for transitions
    void StartAnimation(Math::Vec2 startPos, Math::Vec2 endPos, float duration);
    void UpdateAnimation(float dt);
    bool IsAnimating() const { return m_isAnimating; }
    void StopAnimation();

    Math::Matrix GetViewMatrix() const;
    Math::Vec2 GetPosition() const { return m_position; }

    Math::Vec2 GetScreenShakeOffset() const;

    /// 짧은 화면 흔들림 (Train 로봇 착지 등)
    void AddScreenShake(float durationSeconds, float maxPixelOffset);
    void UpdateScreenShake(float dt);

private:
    Math::Vec2 m_position;
    float m_viewWidth;
    float m_viewHeight;
    Math::Vec2 m_minBounds;
    Math::Vec2 m_maxBounds;
    bool m_hasBounds;

    // Animation state
    bool m_isAnimating = false;
    Math::Vec2 m_animStartPos;
    Math::Vec2 m_animEndPos;
    float m_animDuration = 0.0f;
    float m_animElapsed = 0.0f;

    float m_shakeRemain    = 0.f;
    float m_shakeDuration  = 0.f;
    float m_shakePeakPx    = 0.f;
    float m_shakePhase     = 0.f;
};